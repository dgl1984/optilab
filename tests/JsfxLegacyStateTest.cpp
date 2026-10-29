#include "OptiLabCore.h"
#include <ysfx.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
int main(int argc,char**argv) {
 try {
  if(argc!=3)throw std::runtime_error("Usage: jsfx-legacy-state SCRIPT REPORT");
  std::ofstream report(argv[2]);if(!report)throw std::runtime_error("Cannot open report");
  report.precision(17);report<<"case\tmax_error\tnrmse\tnative_latency\tjsfx_latency\n";
  auto config=std::unique_ptr<ysfx_config_t,decltype(&ysfx_config_free)>(ysfx_config_new(),ysfx_config_free);
  if(!config)throw std::runtime_error("Cannot create runtime");
  using Fx=std::unique_ptr<ysfx_t,decltype(&ysfx_free)>;
  using State=std::unique_ptr<ysfx_state_t,decltype(&ysfx_state_free)>;
  auto create=[&](){Fx fx(ysfx_new(config.get()),ysfx_free);if(!fx||!ysfx_load_file(fx.get(),argv[1],0)||!ysfx_compile(fx.get(),ysfx_compile_no_gfx))throw std::runtime_error("JSFX compilation failed");ysfx_set_sample_rate(fx.get(),48000);ysfx_set_block_size(fx.get(),64);ysfx_init(fx.get());return fx;};
  int cases=0,failures=0;
  for(int mode=0;mode<3;++mode)for(double adapt:{0.,41.25,100.})for(double drive:{-7.,2.75,17.}) {
   auto p=OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(mode));p.autoAdaptPct=adapt;p.inputDriveDb=drive;
   auto original=create();ysfx_slider_set_value(original.get(),0,mode==0?1:mode==1?0:2);ysfx_slider_set_value(original.get(),1,drive);ysfx_slider_set_value(original.get(),2,adapt);
   State state(ysfx_save_state(original.get()),ysfx_state_free);if(!state)throw std::runtime_error("Save failed");
   // Earlier sessions saved the sliders without a custom @serialize payload.
   state->data_size=0;
   auto restored=create();
   // Recall into a running instance on a different mode, as a host can do.
   ysfx_slider_set_value(restored.get(),0,mode==0?0:1);
   ysfx_slider_set_value(restored.get(),1,6.125);
   ysfx_slider_set_value(restored.get(),2,69);
   float warmL[64],warmR[64],warmOutL[64]{},warmOutR[64]{};
   for(int i=0;i<64;++i){warmL[i]=float(.2*std::sin(i*.13));warmR[i]=float(.1*std::cos(i*.07));}
   const float*warmInputs[]{warmL,warmR};float*warmOutputs[]{warmOutL,warmOutR};
   ysfx_process_float(restored.get(),warmInputs,warmOutputs,2,2,64);
   if(!ysfx_load_state(restored.get(),state.get()))throw std::runtime_error("Legacy restore failed");
   OptiLabCore core;core.setParameters(p);core.prepare(48000);
   double maximum=0,error=0,energy=0;
   for(int pos=0;pos<8192;pos+=64) {
    float l[64],r[64],jl[64]{},jr[64]{},nl[64],nr[64];
    for(int i=0;i<64;++i){int t=pos+i;l[i]=float(.18*std::sin(t*.13)+.12*std::sin(t*.019)+(t%997==0?.2:0));r[i]=float(.21*std::sin(t*.071));auto n=core.processSample(l[i],r[i]);nl[i]=n.first;nr[i]=n.second;}
    const float*inputs[]{l,r};float*outputs[]{jl,jr};ysfx_process_float(restored.get(),inputs,outputs,2,2,64);
    for(int i=0;i<64;++i){if(!std::isfinite(jl[i])||!std::isfinite(jr[i]))throw std::runtime_error("Nonfinite output");double dl=double(jl[i])-nl[i],dr=double(jr[i])-nr[i];maximum=std::max({maximum,std::abs(dl),std::abs(dr)});error+=dl*dl+dr*dr;energy+=double(nl[i])*nl[i]+double(nr[i])*nr[i];}
   }
   double normalized=std::sqrt(error/std::max(energy,1e-30));auto delay=ysfx_get_pdc_delay(restored.get());
   report<<"legacy/"<<mode<<'/'<<adapt<<'/'<<drive<<'\t'<<maximum<<'\t'<<normalized<<'\t'<<core.latencySamples()<<'\t'<<delay<<'\n';
   ++cases;if(maximum>1e-6||normalized>1e-6||core.latencySamples()!=delay)++failures;
  }
  std::cout<<cases<<" legacy slider-only session cases, "<<failures<<" failed\n";return failures?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}
}
