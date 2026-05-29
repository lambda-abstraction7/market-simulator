
# CppTrader: Limit Order Book Simulator

![C++17](https://img.shields.io/badge/C++-17-blue.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

A high-performance C++ matching engine and algorithmic trading simulator. CppTrader models price-time priority limit/market order matching, realistic partial fills, real-time portfolio mark-to-market tracking, and strategy execution over historical OHLCV data.

<img width="800" height="520" alt="ScreenRecording2026-05-25at11 55 45-ezgif com-video-to-gif-converter" src="https://github.com/user-attachments/assets/b5701368-2b1a-4ae6-971f-6667bd18f52a" />

---

## Features

- **Order Book Engine:** Price-time priority matching with separate mapping for bids (highest first) and asks (lowest first).
- **Order Types:** Full support for both Market and Limit orders with exact partial fill handling.
- **Portfolio Tracking:** Real-time accounting of cash balance, active inventory (position), and total mark-to-market equity.
- **Order Management (OMS):** Active tracking and cancellation of unfilled limit orders to prevent "zombie" executions and accidental short-selling.
- **Simulated Liquidity:** "Noise trader" injection to populate the book and simulate realistic spread dynamics.
- **CSV Data Feed:** Load and iterate through historical OHLCV bar data.

---

## Project Structure

```text
.
├── data/
│   └── prices.csv        # Historical OHLCV price data
├── src/
│   └── main.cpp          # Core simulation engine
└── README.md
