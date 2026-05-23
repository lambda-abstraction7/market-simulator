#include <algorithm>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum class Side { BUY, SELL };

string sideToString(Side s) { return s == Side::BUY ? "BUY" : "SELL"; }

enum class OrderType { LIMIT, MARKET };

struct Order {
  int id;
  Side side;
  double price;
  int quantity;
  bool isMine;
  OrderType type = OrderType::LIMIT;
};

struct Bar {
  string date;
  double open, high, low, close;
  long volume;
};

struct Portfolio {
  double cash;
  int position;
  double totalPnl;

  Portfolio(double startingCash)
      : cash(startingCash), position(0), totalPnl(0.0) {}

  void onTrade(Side side, double price, int qty) {
    if (side == Side::BUY) {
      cash -= price * qty;
      position += qty;
    } else {
      cash += price * qty;
      position -= qty;
    }
  }

  double equity(double currentPrice) const {
    return cash + (position * currentPrice);
  }

  void printStatus(double currentPrice) const {
    double eq = equity(currentPrice);
    cout << fixed << setprecision(2);
    cout << "  Cash: $" << cash << " | Position: " << position << " units"
         << " | Equity: $" << eq << endl;
  }
};

struct DataFeed {
  vector<Bar> bars;
  void loadCSV(const string &filepath) {
    ifstream file(filepath);

    if (!file.is_open()) {
      cout << "ERROR: Could not open file: " << filepath << endl;
      return;
    }
    string line;
    getline(file, line);

    while (getline(file, line)) {
      stringstream ss(line);
      string token;
      Bar bar;
      getline(ss, bar.date, ',');
      getline(ss, token, ',');
      bar.open = stod(token);
      getline(ss, token, ',');
      bar.high = stod(token);
      getline(ss, token, ',');
      bar.low = stod(token);
      getline(ss, token, ',');
      bar.close = stod(token);
      getline(ss, token, ',');
      bar.volume = stol(token);
      bars.push_back(bar);
    }
    cout << "Loaded " << bars.size() << " bars." << endl;
  }
};

struct OrderBook {

  map<double, deque<Order>, greater<double>> bids;
  map<double, deque<Order>> asks;

  void addOrder(const Order &o) {
    if (o.side == Side::BUY)
      bids[o.price].push_back(o);
    else
      asks[o.price].push_back(o);
  }

  void matchOrders(Portfolio &portfolio) {
    while (!bids.empty() && !asks.empty()) {
      auto bestBidIt = bids.begin();
      auto bestAskIt = asks.begin();

      double bidPrice = bestBidIt->first;
      double askPrice = bestAskIt->first;

      if (bidPrice < askPrice) {
        break; 
      }

      Order &bidOrder = bestBidIt->second.front();
      Order &askOrder = bestAskIt->second.front();

      int tradeQty = std::min(bidOrder.quantity, askOrder.quantity);
      double tradePrice = askPrice;

      if (bidOrder.isMine || askOrder.isMine) {
          cout << "  LIMIT TRADE: " << tradeQty << " units at $" << tradePrice
               << "  (Buyer #" << bidOrder.id << " <-> Seller #" << askOrder.id << ")" << endl;
      }

      if (bidOrder.isMine) portfolio.onTrade(Side::BUY, tradePrice, tradeQty);
      if (askOrder.isMine) portfolio.onTrade(Side::SELL, tradePrice, tradeQty);

      bidOrder.quantity -= tradeQty;
      askOrder.quantity -= tradeQty;

      if (bidOrder.quantity == 0) {
        bestBidIt->second.pop_front();
        if (bestBidIt->second.empty()) bids.erase(bestBidIt);
      }

      if (askOrder.quantity == 0) {
        bestAskIt->second.pop_front();
        if (bestAskIt->second.empty()) asks.erase(bestAskIt);
      }
    }
  }

  void printBook() {
    cout << "\n--- ORDER BOOK ---" << endl;
    cout << "ASKS:" << endl;
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
      for (const Order &o : it->second) {
        cout << "  #" << o.id << " | $" << it->first << " | Qty: " << o.quantity << endl;
      }
    }
    cout << "       --- spread ---" << endl;
    cout << "BIDS:" << endl;
    for (auto &pair : bids) {
      double price = pair.first;
      for (const Order &o : pair.second) {
        cout << "  #" << o.id << " | $" << price << " | Qty: " << o.quantity << endl;
      }
    }
  }

  bool cancelOrder(Side side, double price, int orderId) {
    if (side == Side::BUY) {
      auto levelIt = bids.find(price);
      if (levelIt != bids.end()) {
        auto &queue = levelIt->second;
        for (auto qIt = queue.begin(); qIt != queue.end(); ++qIt) {
          if (qIt->id == orderId) {
            queue.erase((qIt));
            if (queue.empty()) bids.erase(levelIt);
            return true;
          }
        }
      }
    } else {
      auto levelIt = asks.find(price);
      if (levelIt != asks.end()) {
        auto &queue = levelIt->second;
        for (auto qIt = queue.begin(); qIt != queue.end(); ++qIt) {
          if (qIt->id == orderId) {
            queue.erase(qIt);
            if (queue.empty()) asks.erase((levelIt));
            return true;
          }
        }
      }
    }
    return false;
  }

  void submitMarketOrder(Order &mo, Portfolio &portfolio) {
    if (mo.side == Side::BUY) {
      while (mo.quantity > 0 && !asks.empty()) {
        auto bestAskIt = asks.begin();
        double askPrice = bestAskIt->first;
        Order &restingAsk = bestAskIt->second.front();

        int tradeQty = std::min(mo.quantity, restingAsk.quantity);

        cout << "  MARKET TRADE: " << tradeQty << " units at $" << askPrice
             << "  (Market Buyer #" << mo.id << " <-> Limit Seller #" << restingAsk.id << ")" << endl;

        if (mo.isMine) portfolio.onTrade(Side::BUY, askPrice, tradeQty);
        if (restingAsk.isMine) portfolio.onTrade(Side::SELL, askPrice, tradeQty);

        mo.quantity -= tradeQty;
        restingAsk.quantity -= tradeQty;

        if (restingAsk.quantity == 0) {
          bestAskIt->second.pop_front();
          if (bestAskIt->second.empty()) asks.erase(bestAskIt);
        }
      }
      if (mo.quantity > 0) {
        cout << "  Market Order #" << mo.id << " partially filled. Remaining "
             << mo.quantity << " canceled (no liquidity)." << endl;
      }
    } else {
      while (mo.quantity > 0 && !bids.empty()) {
        auto bestBidIt = bids.begin();
        double bidPrice = bestBidIt->first;
        Order &restingBid = bestBidIt->second.front();

        int tradeQty = std::min(mo.quantity, restingBid.quantity);

        cout << "  MARKET TRADE: " << tradeQty << " units at $" << bidPrice
             << "  (Limit Buyer #" << restingBid.id << " <-> Market Seller #" << mo.id << ")" << endl;

        if (restingBid.isMine) portfolio.onTrade(Side::BUY, bidPrice, tradeQty);
        if (mo.isMine) portfolio.onTrade(Side::SELL, bidPrice, tradeQty);

        mo.quantity -= tradeQty;
        restingBid.quantity -= tradeQty;

        if (restingBid.quantity == 0) {
          bestBidIt->second.pop_front();
          if (bestBidIt->second.empty()) bids.erase(bestBidIt);
        }
      }
      if (mo.quantity > 0) {
        cout << "  Market Order #" << mo.id << " partially filled. Remaining "
             << mo.quantity << " canceled (no liquidity)." << endl;
      }
    }
  }
};

struct NoiseGenerator {
  mt19937 rng;
  NoiseGenerator() : rng(42) {}

  vector<Order> generateNoise(double closePrice, int &nextId) {
    vector<Order> noiseOrders;
    uniform_real_distribution<double> offset(0.5, 2.0);
    uniform_int_distribution<int> qty(5, 20);

    double bidPrice = closePrice - offset(rng);
    noiseOrders.push_back({nextId++, Side::BUY, bidPrice, qty(rng), false});

    double askPrice = closePrice + offset(rng);
    noiseOrders.push_back({nextId++, Side::SELL, askPrice, qty(rng), false});

    return noiseOrders;
  }
};

optional<Order> generateOrder(const vector<Bar> &bars, int i, int &nextId, const Portfolio &portfolio) {
  bool priceRose = bars[i].close > bars[i - 1].close;

  if (priceRose && portfolio.position == 0)
    return Order{nextId++, Side::BUY, 0.0, 10, true, OrderType::MARKET}; 

  if (!priceRose && portfolio.position > 0)
    return Order{nextId++, Side::SELL, 0.0, portfolio.position, true, OrderType::MARKET};

  return nullopt;
}

int main() {
  DataFeed feed;
  feed.loadCSV("../data/prices.csv");

  OrderBook book;
  Portfolio portfolio(10000.00);
  NoiseGenerator noise;
  int nextId = 1;
  vector<Order> myActiveOrders;

  cout << "\n--- SIMULATION ---" << endl;

  for (int i = 1; i < (int)feed.bars.size(); i++) {
    const Bar &bar = feed.bars[i];
    cout << "\nBar " << bar.date << " | close: $" << fixed << setprecision(2) << bar.close << endl;


    for (const Order &o : noise.generateNoise(bar.close, nextId)) {
        book.addOrder(o);
    }

    for (const Order &oldOrder : myActiveOrders) {
      bool canceled = book.cancelOrder(oldOrder.side, oldOrder.price, oldOrder.id);
      if (canceled) {
        cout << "  CANCELED old limit order #" << oldOrder.id << endl;
      }
    }
    myActiveOrders.clear();

    auto order = generateOrder(feed.bars, i, nextId, portfolio);
    if (order.has_value()) {
        if (order->type == OrderType::MARKET) {
            cout << "Signal: " << sideToString(order->side) << " MARKET order" << endl;
            book.submitMarketOrder(*order, portfolio); 
        } else {
            cout << "Signal: " << sideToString(order->side) << " LIMIT order at $" << order->price << endl;
            book.addOrder(*order);
            myActiveOrders.push_back(*order);
        }
    } else {
        cout << "Signal: none" << endl;
    }

    book.matchOrders(portfolio);
    portfolio.printStatus(bar.close);
  }

  if (!feed.bars.empty()) {
      const Bar &last = feed.bars.back();
      cout << "\n--- FINAL PORTFOLIO ---" << endl;
      cout << fixed << setprecision(2);
      cout << "Cash:     $" << portfolio.cash << endl;
      cout << "Position: " << portfolio.position << " units" << endl;
      cout << "Equity:   $" << portfolio.equity(last.close) << endl;
  }

  book.printBook();
  return 0;
}