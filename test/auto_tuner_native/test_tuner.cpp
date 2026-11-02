#include <vector>
#include <cassert>
#include <iostream>
#include <cmath>
#include "auto_tuner.h"
#include "extranonce_range.h"
int64_t fake_time = 1000000;
std::vector<unsigned char> fake_nvs;
using AutoTuner::JobMode;
JsonDocument state() { JsonDocument doc; AutoTuner::getResultsJson(doc); return doc; }
int main(int argc, char **argv) {
  if (argc > 1 && std::string(argv[1]) == "ranges") {
    for (int bytes=1; bytes<=32; ++bytes) {
      for (int pct : {10000, 7324, 1, 5, 1250, 9999}) {
        for (int i=0; i<(pct+499)/500; ++i) {
          uint8_t low[32]{}, high[32]{}; char l[65], h[65];
          bool valid = ExtranonceRange::bounds(bytes,i,low,high,pct);
          ExtranonceRange::hex(low,bytes,l); ExtranonceRange::hex(high,bytes,h);
          std::cout << "range " << bytes << " " << pct << " " << i << " " << valid << " " << l << " " << h << "\n";
        }
      }
      AutoTuner::init(); AutoTuner::control("reset",10000,0,0); AutoTuner::control("pause");
      char hex[65]; uint32_t run; int step;
      AutoTuner::next(0,bytes,hex,run,step);
      std::cout << "manual " << bytes << " " << hex << "\n";
    }
    for (int bits=1; bits<=256; ++bits) {
      int bytes=(bits+7)/8;
      for (int pct : {10000, 7324, 1, 5, 1250, 9999}) {
        for (int i=0; i<(pct+499)/500; ++i) {
          uint8_t low[32]{}, high[32]{}; char l[65], h[65];
          bool valid=ExtranonceRange::boundsBits(bits,i,low,high,pct);
          ExtranonceRange::hex(low,bytes,l); ExtranonceRange::hex(high,bytes,h);
          std::cout << "bits " << bits << " " << pct << " " << i << " " << valid << " " << l << " " << h << "\n";
        }
      }
      uint8_t val[32]{};char hex[65];
      ExtranonceRange::percentage(bits,7324,val);ExtranonceRange::hex(val,bytes,hex);
      std::cout << "manualbits " << bits << " " << hex << "\n";
    }
    return 0;
  }
  AutoTuner::init();
  char hex[65]; uint32_t run; int step;
  assert(AutoTuner::next(0,4,hex,run,step)==JobMode::Ready);
  assert(std::string(hex)=="BB7E90FE"); // floor(UINT32_MAX * .7324)
  assert(!run && step==-1);
  assert(AutoTuner::next(0,4,hex,run,step)==JobMode::Ready);
  assert(std::string(hex)=="BB7E90FD");
  assert(AutoTuner::control("reset"));
  AutoTuner::next(0,4,hex,run,step); assert(std::string(hex)=="FFFFFFFF");
  uint32_t originalRun=run;
  AutoTuner::next(0,4,hex,run,step); assert(std::string(hex)=="FFFFFFFE");
  AutoTuner::observeHeight(900000);
  for (int i=0;i<100;++i) AutoTuner::observeHeight(900000);
  assert(state()["steps"][0]["blocks"].as<int>()==0);
  for(int i=1;i<=9;++i) { AutoTuner::observeHeight(900000+i); AutoTuner::observeHeight(900000+i); }
  assert(state()["current_step"].as<int>()==0);
  AutoTuner::observeHeight(900010); assert(state()["current_step"].as<int>()==1);
  AutoTuner::next(0,4,hex,run,step); assert(step==1); assert(std::string(hex)=="F3333332");
  AutoTuner::reportShareDiff(originalRun,0,"FFFFFFFF",1e15+0.5);
  AutoTuner::reportShareDiff(run,1,hex,8e12);
  AutoTuner::reportShareDiff(run,1,hex,INFINITY);
  assert(state()["steps"][0]["max_diff"].as<double>()==1e15+0.5);
  assert(state()["steps"][1]["samples"].as<int>()==1);
  for(int i=11;i<=200;++i) AutoTuner::observeHeight(900000+i);
  auto done=state(); assert(done["finished"].as<bool>() && done["enabled"].as<bool>());
  assert(done["current_step"].as<int>()==0 && done["best_step"].as<int>()==0);
  AutoTuner::next(0,4,hex,run,step); assert(run==originalRun && step==0);
  AutoTuner::reportShareDiff(originalRun,1,"F3333332",2e15);
  assert(state()["best_step"].as<int>()==1 && state()["current_step"].as<int>()==1);
  assert(AutoTuner::control("pause"));
  AutoTuner::next(0,4,hex,run,step); assert(std::string(hex)=="BB7E90FE");
  assert(AutoTuner::control("reset",7324,0));
  assert(state()["steps"].size()==15);
  assert(state()["steps"][14]["lower_pct"].as<double>()==0);
  AutoTuner::next(0,8,hex,run,step); assert(strlen(hex)==16 && hex[0]=='B');
  AutoTuner::reportShareDiff(originalRun,0,"FFFFFFFF",9e19);
  assert(state()["steps"][0]["max_diff"].as<double>()==0);
  AutoTuner::reportShareDiff(run,0,hex,1.2e18);
  for(int i=1;i<=8;++i) AutoTuner::reportShareDiff(run,0,hex,i*1e19);
  auto topDoc = state();
  auto top=topDoc["steps"][0]["top"].as<JsonArray>();
  assert(top.size()==5 && top[0]["diff"].as<double>()==8e19);
  assert(AutoTuner::control("pause")); // force checkpoint
  AutoTuner::init();
  assert(state()["start_pct"].as<double>()==73.24);
  assert(state()["steps"][0]["max_diff"].as<double>()==8e19);
  assert(!state()["enabled"].as<bool>());
  assert(!AutoTuner::control("reset",0));
  assert(!AutoTuner::control("reset",10001));
  assert(AutoTuner::control("reset",500));
  AutoTuner::next(0,1,hex,run,step);
  assert(std::string(hex)=="0B");
  for(int i=0;i<11;++i) AutoTuner::next(0,1,hex,run,step);
  assert(std::string(hex)=="00");
  assert(AutoTuner::next(0,1,hex,run,step)==JobMode::Exhausted);
  assert(AutoTuner::control("reset",1));
  assert(AutoTuner::next(0,1,hex,run,step)==JobMode::Exhausted);
  assert(AutoTuner::control("reset",10000));
  assert(AutoTuner::next(0,0,hex,run,step)==JobMode::Off);
  assert(AutoTuner::control("pause"));
  AutoTuner::observeHeight(950000);
  AutoTuner::control("reset");
  AutoTuner::next(0,4,hex,run,step);
  AutoTuner::observeHeight(950001);
  assert(state()["steps"][0]["blocks"].as<int>()==1);
  assert(AutoTuner::control("reset",10000,32,0));
  AutoTuner::next(0,4,hex,run,step,2);
  assert(std::string(hex)=="FFFFFFFF" && run && state()["compatible"].as<bool>());
  assert(AutoTuner::control("reset",10000,8,0));
  AutoTuner::next(0,4,hex,run,step,2);
  assert(std::string(hex)=="000000FF" && run);
  for(int i=0;i<12;++i) AutoTuner::next(0,4,hex,run,step,2);
  assert(std::string(hex)=="000000F3");
  assert(AutoTuner::next(0,4,hex,run,step,2)==JobMode::Exhausted);
  assert(AutoTuner::control("reset",10000,4,0));
  AutoTuner::next(0,4,hex,run,step,1);
  assert(std::string(hex)=="0000000F");
  assert(AutoTuner::next(0,4,hex,run,step,1)==JobMode::Exhausted);
  assert(AutoTuner::control("reset",10000,64,0));
  AutoTuner::next(0,4,hex,run,step,1);
  assert(std::string(hex)=="BB7E90FE" && run==0 && !state()["compatible"].as<bool>());
  for(int i=0;i<20;++i) AutoTuner::observeHeight(960000+i);
  assert(state()["steps"][0]["blocks"].as<int>()==0);
  AutoTuner::next(0,8,hex,run,step,1);
  assert(std::string(hex)=="FFFFFFFFFFFFFFFF" && run && state()["compatible"].as<bool>());
  assert(AutoTuner::control("reset",10000,32,0));
  AutoTuner::next(0,8,hex,run,step,2);
  assert(std::string(hex)=="00000000FFFFFFFF" && run);
  assert(AutoTuner::control("reset",10000,32,2));
  AutoTuner::next(0,4,hex,run,step,1); assert(!run);
  AutoTuner::next(0,4,hex,run,step,2); assert(run);
  assert(AutoTuner::control("reset",10000,32,3));
  assert(AutoTuner::next(0,0,hex,run,step,3)==JobMode::Off && run==0);
  auto standard=state();assert(standard["session_protocol"].as<int>()==3 && !standard["compatible"].as<bool>());
  assert(std::string(standard["current_hex"].as<const char *>()).empty());
  assert(!AutoTuner::control("reset",10000,257));
  assert(!AutoTuner::control("reset",10000,32,4));
  assert(AutoTuner::control("reset",10000,32,2));
  assert(AutoTuner::control("pause"));
  AutoTuner::next(0,8,hex,run,step,2);
  assert(std::string(hex)=="00000000BB7E90FE" && run==0);
  AutoTuner::next(0,8,hex,run,step,2);
  assert(std::string(hex)=="00000000BB7E90FD");
  assert(AutoTuner::control("reset",10000,8,2));
  assert(AutoTuner::control("pause"));
  AutoTuner::next(0,8,hex,run,step,2);
  assert(std::string(hex)=="00000000000000BA");
  for(int i=0;i<186;++i) AutoTuner::next(0,8,hex,run,step,2);
  assert(std::string(hex)=="0000000000000000");
  assert(AutoTuner::next(0,8,hex,run,step,2)==JobMode::Exhausted);
  assert(AutoTuner::control("reset",10000,64,2));
  AutoTuner::control("pause");AutoTuner::init();
  assert(state()["search_bits"].as<int>()==64 && state()["required_protocol"].as<int>()==2);
  assert(AutoTuner::control("pause",-1,-1,-1,5000));
  AutoTuner::next(0,8,hex,run,step,2);
  assert(std::string(hex)=="7FFFFFFFFFFFFFFF");
  AutoTuner::init();assert(state()["manual_pct"].as<double>()==50);
  assert(AutoTuner::control("reset",10000,32,2));
  assert(AutoTuner::control("pause",-1,-1,-1,5000));
  AutoTuner::next(0,8,hex,run,step,2);
  assert(std::string(hex)=="000000007FFFFFFF");
  assert(AutoTuner::control("pause",-1,-1,-1,0));
  AutoTuner::next(0,8,hex,run,step,2);
  assert(std::string(hex)=="0000000000000000");
  assert(AutoTuner::next(0,8,hex,run,step,2)==JobMode::Exhausted);
  assert(!AutoTuner::control("pause",-1,-1,-1,10001));
  assert(!AutoTuner::control("start",-1,-1,-1,5000));
  auto v4=fake_nvs;
  fake_nvs.resize(v4.size()-8);fake_nvs[0]=3;
  AutoTuner::init();
  assert(state()["manual_pct"].as<double>()==73.24 && state()["search_bits"].as<int>()==32);
  fake_nvs=v4;fake_nvs.resize(v4.size()-16);fake_nvs[0]=2;
  AutoTuner::init();
  assert(state()["search_bits"].as<int>()==0 && state()["required_protocol"].as<int>()==0);
  assert(state()["manual_pct"].as<double>()==73.24);
  std::cout << "PASS configurable manual start: selected-domain percentage, padding, zero exhaustion, persistence, v3 migration\n";
  std::cout << "PASS selectable bits/protocols: padding, 32-bit V2 range, mismatch fallback, Standard without extranonce2, v2 checkpoint migration\n";
  std::cout << "PASS tuner: descending, heights, large diffs, delayed results, top 5, hold, manual 73.24%, custom start, persistence, exhaustion\n";
}
