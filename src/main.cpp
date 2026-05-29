#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
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

  Order *next = nullptr;
  Order *prev = nullptr;
};

struct InstructiveOrderList {
  Order *head = nullptr;
  Order *tail = nullptr;
  int count = 0;
  void push_back(Order *o) {
    o->prev = tail;
    o->next = nullptr;
    if (tail)
      tail->next = o;
    else
      head = o;
    tail = o;
    ++count;
  }
  void remove(Order *o) {
    if (o->prev)
      o->prev->next = o->next;
    else
      head = o->next;

    if (o->next)
      o->next->prev = o->prev;
    else
      tail = o->prev;

    o->next = o->prev = nullptr;
    --count;
  }

  Order *front() { return head; }

  void pop_front() {
    if (head)
      remove(head);
  }
};

struct PriceLevelArray {
  static constexpr double TICK = 0.01;
  static constexpr int LEVELS = 10000;
  static constexpr double BASE = 100.0;

  struct Level {
    Order *head = nullptr;
    Order *tail = nullptr;
    int count = 0;
  };
  array<InstructiveOrderList, LEVELS> levels{};

  int bestBid = -1;
  int bestAsk = LEVELS;

  static int toIndex(double price) {
    return static_cast<int>((price - BASE) / TICK + 0.5);
  }
  static double toPrice(int idx) { return BASE + idx * TICK; }
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

  PriceLevelArray bids;
  PriceLevelArray asks;
  unordered_map<int, Order *> orderById;

  void addOrder(Order *o) {
    int idx = PriceLevelArray::toIndex(o->price);

    if (idx < 0 || idx >= PriceLevelArray::LEVELS) {
        cout << "WARNING: Order price $" << o->price << " is out of bounds (Index " << idx << ")! Dropping order." << endl;
        delete o; 
        return;
    }

    if (o->side == Side::BUY) {
      bids.levels[idx].push_back(o);
      if (idx > bids.bestBid)
        bids.bestBid = idx;
    } else {
      asks.levels[idx].push_back(o);
      if (idx < asks.bestAsk)
        asks.bestAsk = idx;
    }
    orderById[o->id] = o;
  }

  bool cancelOrder(int orderId) {
    auto it = orderById.find(orderId);
    if (it == orderById.end())
      return false;
    Order *o = it->second;
    int idx = PriceLevelArray::toIndex(o->price);
    auto &lvl = (o->side == Side::BUY ? bids : asks).levels[idx];
    lvl.remove(o);
    orderById.erase(it);
    return true;
  }

  void matchOrders(Portfolio &portfolio) {

    while (bids.bestBid >= 0 && asks.bestAsk < PriceLevelArray::LEVELS) {

      if (bids.levels[bids.bestBid].count == 0) {
        bids.bestBid--;
        continue;
      }
      if (asks.levels[asks.bestAsk].count == 0) {
        asks.bestAsk++;
        continue;
      }

      double bidPrice = PriceLevelArray::toPrice(bids.bestBid);
      double askPrice = PriceLevelArray::toPrice(asks.bestAsk);

      if (bidPrice < askPrice) {
        break;
      }
      Order *bidOrder = bids.levels[bids.bestBid].front();
      Order *askOrder = asks.levels[asks.bestAsk].front();

      int tradeQty = std::min(bidOrder->quantity, askOrder->quantity);
      double tradePrice = askPrice;

      if (bidOrder->isMine || askOrder->isMine) {
        cout << "  LIMIT TRADE: " << tradeQty << " units at $" << tradePrice
             << "  (Buyer #" << bidOrder->id << " <-> Seller #" << askOrder->id
             << ")" << endl;
      }

      if (bidOrder->isMine)
        portfolio.onTrade(Side::BUY, tradePrice, tradeQty);
      if (askOrder->isMine)
        portfolio.onTrade(Side::SELL, tradePrice, tradeQty);

      bidOrder->quantity -= tradeQty;
      askOrder->quantity -= tradeQty;

      if (bidOrder->quantity == 0) {
        bids.levels[bids.bestBid].pop_front();
        orderById.erase(bidOrder->id); 
        delete bidOrder;               
      }
      if (askOrder->quantity == 0) {
        asks.levels[asks.bestAsk].pop_front();
        orderById.erase(askOrder->id); 
        delete askOrder;               
      }
    }
  }


void printBook() {
  cout << "\n--- ORDER BOOK ---" << endl;
  cout << "ASKS:" << endl;
  for (int i = PriceLevelArray::LEVELS - 1; i >= asks.bestAsk; --i) {
    Order *curr = asks.levels[i].head;
    double price = PriceLevelArray::toPrice(i);
    while (curr) {
      cout << "  #" << curr->id << " | $" << price
           << " | Qty: " << curr->quantity << endl;
      curr = curr->next;
    }
  }
  cout << "       --- spread ---" << endl;
  cout << "BIDS:" << endl;
  for (int i = bids.bestBid; i >= 0; --i) {
    Order *curr = bids.levels[i].head;
    double price = PriceLevelArray::toPrice(i);
    while (curr) {
      cout << "  #" << curr->id << " | $" << price
           << " | Qty: " << curr->quantity << endl;
      curr = curr->next;
    }
  }
}

void submitMarketOrder(Order *mo, Portfolio &portfolio) {
  if (mo->side == Side::BUY) {
    while (mo->quantity > 0 && asks.bestAsk < PriceLevelArray::LEVELS) {

      InstructiveOrderList &askQueue = asks.levels[asks.bestAsk];
      if (askQueue.count == 0) {
        asks.bestAsk++;
        continue;
      }

      double askPrice = PriceLevelArray::toPrice(asks.bestAsk);
      Order *restingAsk = askQueue.front();
      int tradeQty = std::min(mo->quantity, restingAsk->quantity);

      if (mo->isMine)
        portfolio.onTrade(Side::BUY, askPrice, tradeQty);
      if (restingAsk->isMine)
        portfolio.onTrade(Side::SELL, askPrice, tradeQty);

      mo->quantity -= tradeQty;
      restingAsk->quantity -= tradeQty;

      if (restingAsk->quantity == 0) {
        askQueue.pop_front();
        orderById.erase(restingAsk->id); 
        delete restingAsk;              
      }
    }
  } else {

    while (mo->quantity > 0 && bids.bestBid >= 0) {

      InstructiveOrderList &bidQueue = bids.levels[bids.bestBid];
      if (bidQueue.count == 0) {
        bids.bestBid--;
        continue;
      }

      double bidPrice = PriceLevelArray::toPrice(bids.bestBid);
      Order *restingBid = bidQueue.front();
      int tradeQty = std::min(mo->quantity, restingBid->quantity);

      if (mo->isMine)
        portfolio.onTrade(Side::SELL, bidPrice, tradeQty);
      if (restingBid->isMine)
        portfolio.onTrade(Side::BUY, bidPrice, tradeQty);

      mo->quantity -= tradeQty;
      restingBid->quantity -= tradeQty;

      if (restingBid->quantity == 0) {
        bidQueue.pop_front();
        orderById.erase(restingBid->id); 
        delete restingBid;              
      }
    }
  }
  delete mo;
}
};

struct NoiseGenerator {
  mt19937 rng;
  NoiseGenerator() : rng(42) {}

  vector<Order *> generateNoise(double closePrice, int &nextId) {
    vector<Order *> noiseOrders;
    uniform_real_distribution<double> offset(0.5, 2.0);
    uniform_int_distribution<int> qty(5, 20);

    double bidPrice = closePrice - offset(rng);
    noiseOrders.push_back(
        new Order{nextId++, Side::BUY, bidPrice, qty(rng), false});

    double askPrice = closePrice + offset(rng);
    noiseOrders.push_back(
        new Order{nextId++, Side::SELL, askPrice, qty(rng), false});

    return noiseOrders;
  }
};

Order* generateOrder(const vector<Bar> &bars, int i, int &nextId, const Portfolio &portfolio) {
  bool priceRose = bars[i].close > bars[i - 1].close;

  if (priceRose && portfolio.position == 0)
    return new Order{nextId++, Side::BUY, 0.0, 10, true, OrderType::MARKET};

  if (!priceRose && portfolio.position > 0)
    return new Order{nextId++, Side::SELL, 0.0, portfolio.position, true, OrderType::MARKET};

  return nullptr;
}

int main() {
  DataFeed feed;
  feed.loadCSV("../data/prices.csv");

  OrderBook book;
  Portfolio portfolio(10000.00);
  NoiseGenerator noise;
  int nextId = 1;
  vector<Order*> myActiveOrders;

  cout << "\n--- SIMULATION ---" << endl;

  for (int i = 1; i < (int)feed.bars.size(); i++) {
    const Bar &bar = feed.bars[i];
    cout << "\nBar " << bar.date << " | close: $" << fixed << setprecision(2)
         << bar.close << endl;

    for (Order* o : noise.generateNoise(bar.close, nextId)) {
      book.addOrder(o);
    }

    for (Order* oldOrder : myActiveOrders) {
      bool canceled = book.cancelOrder(oldOrder->id); 
        if(canceled) {
          cout << "  CANCELED old limit order #" << oldOrder->id << endl;
        delete oldOrder;
        } 
    }
    myActiveOrders.clear();

    Order* order = generateOrder(feed.bars, i, nextId, portfolio);
    if (order != nullptr) {
      if (order->type == OrderType::MARKET) {
        cout << "Signal: " << sideToString(order->side) << " MARKET order" << endl;
        book.submitMarketOrder(order, portfolio);
      } else {
        cout << "Signal: " << sideToString(order->side) << " LIMIT order at $" << order->price << endl;
        book.addOrder(order);
        myActiveOrders.push_back(order);
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