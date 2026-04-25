#pragma once
// ===========================================================================
// TradeHistory.h — Full Trade History System
// ===========================================================================
// Dependensi:
//   - Forward declare LiveTrade (definisi di TradeModule.h)
//
// Di TradeModule.h:
//   1. #include "TradeHistory.h"
//   2. Setelah class LiveTrade, taruh inline RecordClosed()
// ===========================================================================

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstdarg>

// ============================================================
// Simple JSON value reader (tanpa dependensi nlohmann)
// Cukup untuk parse format yang dihasilkan SaveToFile()
// ============================================================
static double jsonGetNum(const std::string& key, const std::string& jsonChunk) {
    std::string search = "\"" + key + "\":";
    size_t pos = jsonChunk.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.length();
    while (pos < jsonChunk.size() && (jsonChunk[pos] == ' ' || jsonChunk[pos] == '\t')) pos++;
    if (pos >= jsonChunk.size()) return 0;
    if (jsonChunk[pos] == '"') {
        pos++;
        size_t end = jsonChunk.find('"', pos);
        if (end == std::string::npos) return 0;
        return std::stod(jsonChunk.substr(pos, end - pos));
    }
    size_t end = pos;
    while (end < jsonChunk.size() && jsonChunk[end] != ',' && jsonChunk[end] != '}' && jsonChunk[end] != '\n') end++;
    return std::stod(jsonChunk.substr(pos, end - pos));
}

static std::string jsonGetStr(const std::string& key, const std::string& jsonChunk) {
    std::string search = "\"" + key + "\":";
    size_t pos = jsonChunk.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    while (pos < jsonChunk.size() && (jsonChunk[pos] == ' ' || jsonChunk[pos] == '\t')) pos++;
    if (pos >= jsonChunk.size() || jsonChunk[pos] != '"') return "";
    pos++;
    size_t end = jsonChunk.find('"', pos);
    if (end == std::string::npos) return "";
    return jsonChunk.substr(pos, end - pos);
}

// ============================================================
// FORWARD DECLARATION — LiveTrade didefinisikan di TradeModule.h
// Implementasi RecordClosed() diletakkan di TradeModule.h
// (setelah class LiveTrade selesai didefinisikan)
// ============================================================
class LiveTrade;

// ============================================================
// ENUM: TH_TradeType (namespace terpisah dari TradeModule)
// ============================================================
enum TH_TradeType {
    TH_TRADE_BUY  = 0,
    TH_TRADE_SELL = 1
};

// ============================================================
// HELPER FUNCTIONS (Self-contained)
// ============================================================
static std::string TH_FormatUSD(double val) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << val;
    return ss.str();
}

static std::string TH_FormatTime(double timestamp) {
    if (timestamp <= 0) return "-";
    time_t t = (time_t)timestamp;
    struct tm tm;
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return std::string(buf);
}

// Format detail dengan detik (untuk tooltip)
static std::string TH_FormatTimeLong(double timestamp) {
    if (timestamp <= 0) return "-";
    time_t t = (time_t)timestamp;
    struct tm tm;
    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif
    char buf[48];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

// ============================================================
// STRUCT: ClosedTrade — Setiap trade yang sudah ditutup
// ============================================================
struct ClosedTrade {
    int         id          = 0;
    std::string symbol      = "";
    TH_TradeType type       = TH_TRADE_BUY;
    std::string typeStr     = "BUY";     // "BUY" atau "SELL"
    double      entryPrice  = 0;
    double      closePrice  = 0;
    double      profit      = 0;
    std::string closeReason = "";
    double      openTime    = 0;
    double      closeTime   = 0;
    double      duration    = 0;         // detik (closeTime - openTime)
    double      volume      = 0;
    double      sl          = 0;
    double      tp          = 0;

    // Hitung RR ratio yang tereksekusi
    double GetRRRatio() const {
        double risk = fabs(entryPrice - sl);
        double reward = fabs(closePrice - entryPrice);
        if (risk < 0.00001) return 0.0;
        return reward / risk;
    }
};

// ============================================================
// STRUCT: TradeStats — Statistik kalkulasi
// ============================================================
struct TradeStats {
    int totalTrades     = 0;
    int wins            = 0;
    int losses          = 0;
    int breakevens      = 0;
    double winRate      = 0;

    double avgWin       = 0;
    double avgLoss      = 0;
    double avgTrade     = 0;
    double largestWin   = 0;
    double largestLoss  = 0;

    double grossProfit  = 0;
    double grossLoss    = 0;
    double netProfit    = 0;
    double profitFactor = 0;

    int maxConsecWin    = 0;
    int maxConsecLoss   = 0;

    double avgRR        = 0;

    // Per-symbol breakdown
    std::map<std::string, int>    tradesPerSymbol;
    std::map<std::string, double> pnlPerSymbol;
};

// ============================================================
// CLASS: TradeHistory — Menyimpan semua trade yang sudah close
// ============================================================
class TradeHistory {
public:
    std::vector<ClosedTrade> closedTrades;

    // --------------------------------------------------------
    // DECLARATION saja.
    // IMPLEMENTASI ada di TradeModule.h (setelah class LiveTrade)
    // agar compiler sudah kenal semua member LiveTrade.
    // --------------------------------------------------------
    void RecordClosed(const LiveTrade& t, const char* reason, double closeTime);

    // --- Stats ---
    TradeStats CalculateStats() const {
        TradeStats s;
        s.totalTrades = (int)closedTrades.size();
        if (s.totalTrades == 0) return s;

        int tempConsecW = 0, tempConsecL = 0;
        double sumWin = 0, sumLoss = 0;
        double sumRR = 0;

        for (const auto& t : closedTrades) {
            if (t.profit > 0.0001) {
                s.wins++;
                sumWin += t.profit;
                s.grossProfit += t.profit;
                tempConsecW++;
                tempConsecL = 0;
            } else if (t.profit < -0.0001) {
                s.losses++;
                sumLoss += t.profit;
                s.grossLoss += fabs(t.profit);
                tempConsecL++;
                tempConsecW = 0;
            } else {
                s.breakevens++;
                tempConsecW = 0;
                tempConsecL = 0;
            }

            if (tempConsecW > s.maxConsecWin) s.maxConsecWin = tempConsecW;
            if (tempConsecL > s.maxConsecLoss) s.maxConsecLoss = tempConsecL;

            if (t.profit > s.largestWin)  s.largestWin  = t.profit;
            if (t.profit < s.largestLoss) s.largestLoss = t.profit;

            s.netProfit += t.profit;
            sumRR += t.GetRRRatio();

            s.tradesPerSymbol[t.symbol]++;
            s.pnlPerSymbol[t.symbol] += t.profit;
        }

        s.winRate = (double)s.wins / (double)s.totalTrades * 100.0;
        s.avgWin   = (s.wins  > 0) ? sumWin  / s.wins  : 0;
        s.avgLoss  = (s.losses > 0) ? sumLoss / s.losses : 0;
        s.avgTrade = s.netProfit / s.totalTrades;
        s.avgRR    = sumRR / s.totalTrades;
        s.profitFactor = (s.grossLoss > 0.0001) ? s.grossProfit / s.grossLoss : 0;

        return s;
    }

    // --- Save ---
    void SaveToFile(const std::string& path) const {
        std::ofstream o(path);
        if (!o.is_open()) {
            std::cerr << "[TradeHistory] Gagal buka file: " << path << std::endl;
            return;
        }

        o << "[\n";
        for (size_t i = 0; i < closedTrades.size(); i++) {
            const auto& t = closedTrades[i];
            o << "  {\n";
            o << "    \"id\": "         << t.id         << ",\n";
            o << "    \"symbol\": \""   << t.symbol     << "\",\n";
            o << "    \"type\": \""     << t.typeStr    << "\",\n";
            o << "    \"entryPrice\": " << t.entryPrice << ",\n";
            o << "    \"closePrice\": " << t.closePrice << ",\n";
            o << "    \"profit\": "     << t.profit     << ",\n";
            o << "    \"closeReason\": \"" << t.closeReason << "\",\n";
            o << "    \"openTime\": "   << t.openTime   << ",\n";
            o << "    \"closeTime\": "  << t.closeTime  << ",\n";
            o << "    \"duration\": "   << t.duration   << ",\n";
            o << "    \"volume\": "     << t.volume     << ",\n";
            o << "    \"sl\": "         << t.sl         << ",\n";
            o << "    \"tp\": "         << t.tp         << "\n";
            o << "  }";
            if (i + 1 < closedTrades.size()) o << ",";
            o << "\n";
        }
        o << "]\n";
        o.close();

        std::cout << "[TradeHistory] Saved " << closedTrades.size()
                  << " records to " << path << std::endl;
    }

    // --- Load ---
    void LoadFromFile(const std::string& path) {
        std::ifstream i(path);
        if (!i.is_open()) {
            std::cout << "[TradeHistory] File tidak ditemukan: " << path << std::endl;
            return;
        }

        std::string content((std::istreambuf_iterator<char>(i)),
                             std::istreambuf_iterator<char>());
        i.close();

        if (content.size() < 3) return;

        size_t pos = 0;
        int loaded = 0;

        while ((pos = content.find("{", pos)) != std::string::npos) {
            size_t end = content.find("}", pos);
            if (end == std::string::npos) break;

            std::string chunk = content.substr(pos, end - pos + 1);

            ClosedTrade rec;
            rec.id          = (int)jsonGetNum("id", chunk);
            rec.symbol      = jsonGetStr("symbol", chunk);
            rec.typeStr     = jsonGetStr("type", chunk);
            rec.type        = (rec.typeStr == "SELL") ? TH_TRADE_SELL : TH_TRADE_BUY;
            rec.entryPrice  = jsonGetNum("entryPrice", chunk);
            rec.closePrice  = jsonGetNum("closePrice", chunk);
            rec.profit      = jsonGetNum("profit", chunk);
            rec.closeReason = jsonGetStr("closeReason", chunk);
            rec.openTime    = jsonGetNum("openTime", chunk);
            rec.closeTime   = jsonGetNum("closeTime", chunk);
            rec.duration    = jsonGetNum("duration", chunk);
            rec.volume      = jsonGetNum("volume", chunk);
            rec.sl          = jsonGetNum("sl", chunk);
            rec.tp          = jsonGetNum("tp", chunk);

            closedTrades.push_back(rec);
            loaded++;

            pos = end + 1;
        }

        std::cout << "[TradeHistory] Loaded " << loaded
                  << " records from " << path << std::endl;
    }

    // --- Export CSV ---
    void ExportCSV(const std::string& path) const {
        std::ofstream o(path);
        if (!o.is_open()) {
            std::cerr << "[TradeHistory] Gagal buka file: " << path << std::endl;
            return;
        }

        o << "ID,Symbol,Type,Entry,Close,SL,TP,Profit,Reason,OpenTime,CloseTime,Duration,Volume\n";

        for (const auto& t : closedTrades) {
            o << t.id << ","
              << t.symbol << ","
              << t.typeStr << ","
              << std::fixed << std::setprecision(2)
              << t.entryPrice << ","
              << t.closePrice << ","
              << t.sl << ","
              << t.tp << ","
              << t.profit << ","
              << "\"" << t.closeReason << "\","
              << t.openTime << ","
              << t.closeTime << ","
              << t.duration << ","
              << t.volume << "\n";
        }
        o.close();

        std::cout << "[TradeHistory] Exported " << closedTrades.size()
                  << " records to " << path << std::endl;
    }

    void Clear() {
        closedTrades.clear();
    }

    int TotalClosed() const { return (int)closedTrades.size(); }
};
