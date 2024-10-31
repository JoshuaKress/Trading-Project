#include <Trade\Trade.mqh>

// Inputs
input int InpRangeStart = 600;         // RANGE START TIME IN MINUTES
input long InputMagicNumber = 12345;   // Magic number
input double Inputlots = 1.0;          // Lot size
input int InputRangeDuration = 120;    // Range duration
input int InputRangeClose = 1200;      // Range close time in minutes
input int InpStopLoss = 450;           // SL % of range
input int InpTakeProfit = 80;          // TP % of range

struct RANGE_STRUCT {
   datetime start_time;     // Start of the range
   datetime end_time;       // End of range
   datetime close_time;     // Close time
   double high;             // High of range
   double low;              // Low of range
   bool f_entry;            // Flag if inside of the range
   bool f_high_breakout;    // Flag if high breakout occurred
   bool f_low_breakout;     // Flag if low breakout occurred

   RANGE_STRUCT(): start_time(0), end_time(0), close_time(0), high(0), low(999999),
                   f_entry(false), f_high_breakout(false), f_low_breakout(false) {}
};

RANGE_STRUCT range;
MqlTick prevTick;
MqlTick lastTick;
CTrade trade;

int OnInit() {
   trade.SetExpertMagicNumber(InputMagicNumber); // Set magic number
   return (INIT_SUCCEEDED);
}

void OnDeinit(const int reason) {}

// Main function
void OnTick() {
   // Get current tick
   prevTick = lastTick;
   SymbolInfoTick(_Symbol, lastTick);

   // Range calculation
   if (lastTick.time >= range.start_time && lastTick.time < range.end_time) {
      range.f_entry = true;
      // New high
      if (lastTick.ask > range.high) {
         range.high = lastTick.ask;
         drawObjects();
      }
      // New low
      if (lastTick.bid < range.low) {
         range.low = lastTick.bid;
         drawObjects();
      }
   }

   // Close position
   if (lastTick.time >= range.close_time) {
      if (!closePositions()) {
         return;
      }
   }

   // Calculate new range if conditions are met
   if (((InputRangeClose >= 0 && lastTick.time >= range.close_time)
      || (range.f_high_breakout && range.f_low_breakout)
      || (range.end_time == 0)
      || (range.end_time != 0 && lastTick.time > range.end_time && !range.f_entry))
      && countOpenPositions() == 0) {
      calculateRange();
   }

   // Check for breakouts
   checkBreakouts();
}

// Calculate a new range
void calculateRange() {
   // Reset range variables
   range.start_time = 0;
   range.end_time = 0;
   range.close_time = 0;
   range.high = 0.0;
   range.low = 9999999;
   range.f_entry = false;
   range.f_high_breakout = false;
   range.f_low_breakout = false;

   // Calculate range start time
   int time_cycle = 86400;
   range.start_time = (lastTick.time - (lastTick.time % time_cycle)) + InpRangeStart * 60;
   for (int i = 0; i < 8; i++) {
      MqlDateTime tmp;
      TimeToStruct(range.start_time, tmp);
      int DOW = tmp.day_of_week;
      if (lastTick.time >= range.start_time || DOW == 6 || DOW == 0) {
         range.start_time += time_cycle;
      }
   }

   // Range end time
   range.end_time = range.start_time + InputRangeDuration * 60;
   for (int i = 0; i < 2; i++) {
      MqlDateTime tmp;
      TimeToStruct(range.end_time, tmp);
      int DOW = tmp.day_of_week;
      if (DOW == 6 || DOW == 0) {
         range.end_time += time_cycle;
      }
   }

   // Calculate range close time
   range.close_time = (range.end_time - (range.end_time % time_cycle)) + InputRangeClose * 60;
   for (int i = 0; i < 3; i++) {
      MqlDateTime tmp;
      TimeToStruct(range.close_time, tmp);
      int dow = tmp.day_of_week;
      if (range.close_time <= range.end_time || dow == 6 || dow == 0) {
         range.close_time += time_cycle;
      }
   }

   drawObjects();
}

int countOpenPositions() {
   int counter = 0;
   int total = PositionsTotal();
   for (int i = total - 1; i >= 0; i--) {
      ulong ticket = PositionGetTicket(i);
      if (ticket <= 0) { Print("Failed to get position ticket"); return -1; }
      if (!PositionSelectByTicket(ticket)) { Print("Failed to select position by ticket"); return -1; }
      ulong magicNumber;
      if (!PositionGetInteger(POSITION_MAGIC, magicNumber)) { Print("Failed to get position magic number"); return -1; }
      if (InputMagicNumber == magicNumber) { counter++; }
   }
   return counter;
}

void checkBreakouts() {
   if (lastTick.time >= range.end_time && range.end_time > 0 && range.f_entry) {
      // Check for high breakout
      if (!range.f_high_breakout && lastTick.ask >= range.high) {
         range.f_high_breakout = true;

         // Calculate stop loss and take profit
         double sl = NormalizeDouble(lastTick.bid - ((range.high - range.low) * InpStopLoss * 0.01), _Digits);
         double tp = NormalizeDouble(lastTick.ask + ((range.high - range.low) * InpTakeProfit * 0.01), _Digits);

         // Open buy position
         trade.PositionOpen(_Symbol, ORDER_TYPE_BUY, Inputlots, lastTick.ask, sl, tp, "TimeRange EA");
      }

      // Check for low breakout
      if (!range.f_low_breakout && lastTick.bid <= range.low) {
         range.f_low_breakout = true;

         double sl = NormalizeDouble(lastTick.ask + ((range.high - range.low) * InpStopLoss * 0.01), _Digits);
         double tp = NormalizeDouble(lastTick.ask - ((range.high - range.low) * InpTakeProfit * 0.01), _Digits);

         // Open sell position
         trade.PositionOpen(_Symbol, ORDER_TYPE_SELL, Inputlots, lastTick.bid, sl, tp, "TimeRange EA");
      }
   }
}

bool closePositions() {
   int total = PositionsTotal();
   for (int i = total - 1; i >= 0; i--) {
      if (total != PositionsTotal()) { total = PositionsTotal(); i = total; continue; }
      ulong ticket = PositionGetTicket(i);
      if (ticket <= 0) { Print("Failed to get position"); return false; }
      if (!PositionSelectByTicket(ticket)) { Print("Failed to select position by ticket"); return false; }
      long magicNumber;
      if (!PositionGetInteger(POSITION_MAGIC, magicNumber)) { Print("Failed to get position by magic number"); return false; }
      if (magicNumber == InputMagicNumber) {
         trade.PositionClose(ticket);
         if (trade.ResultRetcode() != TRADE_RETCODE_DONE) {
            Print("Failed to close position. Result: " + (string)trade.ResultRetcode() + ":" + trade.ResultRetcodeDescription());
            return false;
         }
      }
   }
   return true;
}

void drawObjects() {
   // Start
   ObjectDelete(NULL, "Range start");
   if (range.start_time > 0) {
      ObjectCreate(NULL, "range start", OBJ_VLINE, 0, range.start_time, 0);
      ObjectSetString(NULL, "range start", OBJPROP_TOOLTIP, "start of the range \n" + TimeToString(range.start_time, TIME_DATE | TIME_MINUTES));
      ObjectSetInteger(NULL, "range start", OBJPROP_COLOR, clrBlue);
      ObjectSetInteger(NULL, "range start", OBJPROP_WIDTH, 2);
      ObjectSetInteger(NULL, "range start", OBJPROP_BACK, true);
   }

   // End time
   ObjectDelete(NULL, "`range end");
   if (range.end_time > 0) {
      ObjectCreate(NULL, "range end", OBJ_VLINE, 0, range.end_time, 0);
      ObjectSetString(NULL, "range end", OBJPROP_TOOLTIP, "end of the range \n" + TimeToString(range.end_time, TIME_DATE | TIME_MINUTES));
      ObjectSetInteger(NULL, "Range end", OBJPROP_COLOR, clrDarkBlue);
      ObjectSetInteger(NULL, "Range end", OBJPROP_WIDTH, 2);
      ObjectSetInteger(NULL, "range end", OBJPROP_BACK, true);
   }

   // Close time
   ObjectDelete(NULL, "`range end");
   if (range.close_time > 0) {
      ObjectCreate(NULL, "range close", OBJ_VLINE, 0, range.close_time, 0);
      ObjectSetString(NULL, "range close", OBJPROP_TOOLTIP, "end of the range \n" + TimeToString(range.close_time, TIME_DATE | TIME_MINUTES));
      ObjectSetInteger(NULL, "Range close", OBJPROP_COLOR, clrGreen);
      ObjectSetInteger(NULL, "Range close", OBJPROP_WIDTH, 2);
      ObjectSetInteger(NULL, "range close", OBJPROP_BACK, true);
   }

   // High line
   ObjectsDeleteAll(NULL, "`range high");
   if (range.high > 0) {
      ObjectCreate(NULL, "range high", OBJ_TREND, 0, range.start_time, range.high, range.end_time, range.high);
      ObjectSetString(NULL, "range high", OBJPROP_TOOLTIP, "end of the range \n" + DoubleToString(range.high, _Digits));
      ObjectSetInteger(NULL, "Range high", OBJPROP_COLOR, clrBlue);
      ObjectSetInteger(NULL, "Range high", OBJPROP_WIDTH, 2);
      ObjectSetInteger(NULL, "range high", OBJPROP_BACK, true);

      // Edit
      ObjectCreate(NULL, "Range High", OBJ_TREND, 0, range.end_time, range.high, range.close_time, range.high);
      ObjectSetString(NULL, "range High", OBJPROP_TOOLTIP, "High of the range\n" + DoubleToString(range.high, _Digits));
      ObjectSetInteger(NULL, "range high", OBJPROP_COLOR, clrBlue);
      ObjectSetInteger(NULL, "range high", OBJPROP_BACK, true);
      ObjectSetInteger(NULL, "range high", OBJPROP_STYLE, STYLE_DOT);
   }

   // Low line
   ObjectsDeleteAll(NULL, "`range low");
   if (range.low < 99999999) {
      ObjectCreate(NULL, "range low", OBJ_TREND, 0, range.start_time, range.low, range.end_time, range.low);
      ObjectSetString(NULL, "range low", OBJPROP_TOOLTIP, "end of the range \n" + DoubleToString(range.low, _Digits));
      ObjectSetInteger(NULL, "Range low", OBJPROP_COLOR, clrGreen);
      ObjectSetInteger(NULL, "Range low", OBJPROP_WIDTH, 2);
      ObjectSetInteger(NULL, "range low", OBJPROP_BACK, true);

      ObjectCreate(NULL, "Range low ", OBJ_TREND, 0, range.end_time, range.low, range.close_time, range.low);
      ObjectSetString(NULL, "range low ", OBJPROP_TOOLTIP, "High of the range\n" + DoubleToString(range.low, _Digits));
      ObjectSetInteger(NULL, "range low ", OBJPROP_COLOR, clrBlue);
      ObjectSetInteger(NULL, "range low ", OBJPROP_BACK, true);
      ObjectSetInteger(NULL, "range low ", OBJPROP_STYLE, STYLE_DOT);
   }
}

