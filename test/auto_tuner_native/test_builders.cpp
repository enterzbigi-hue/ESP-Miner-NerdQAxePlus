#include <pthread.h>
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include "mining_info_v2.h"
#include "mining_utils.h"
#include "hash_capture.h"
std::vector<uint8_t> hashedCoinbase;
MiningInfoBase::~MiningInfoBase() {}
extern "C" void double_sha256_bin(const uint8_t *p, size_t n, uint8_t hash[32]) {
    hashedCoinbase.assign(p, p+n); memset(hash,0,32);
}
extern "C" void swap_endian_words_bin(uint8_t *p, uint8_t *out, size_t n) { memcpy(out,p,n); }
extern "C" void reverse_bytes(uint8_t *, size_t) {}
extern "C" size_t bin2hex(const uint8_t *p,size_t n,char *hex,size_t) {
    const char *digits="0123456789ABCDEF";
    for(size_t i=0;i<n;++i) { hex[2*i]=digits[p[i]>>4]; hex[2*i+1]=digits[p[i]&15]; }
    hex[n*2]=0; return n*2;
}
void destroy(bm_job *j) { free(j->jobid); free(j->extranonce2); free(j); }
int main() {
    MiningInfoV2Extended mi;
    uint8_t prefix[]={0x01,0x02}, suffix[]={0xAA}, assigned[]={0xBE,0xEF};
    sv2_ext_job_t spec{}; spec.job_id=7; spec.ntime=123;
    spec.coinbase_prefix=prefix; spec.coinbase_prefix_len=2;
    spec.coinbase_suffix=suffix; spec.coinbase_suffix_len=1;
    mi.updateJob(&spec,assigned,2,8,0,8192);
    assert(mi.getExtranonceBytes()==8);
    bm_job *j=mi.buildBmJob(0,0,256,"FFFFFFFFFFFFFFFE");
    assert(std::string(j->extranonce2)=="FFFFFFFFFFFFFFFE");
    assert((hashedCoinbase==std::vector<uint8_t>{1,2,0xBE,0xEF,255,255,255,255,255,255,255,254,0xAA}));
    assert(j->capture_inputs_id==42 && HashCapture::capturedCoinbase==hashedCoinbase);
    assert(HashCapture::capturedOffset==4 && HashCapture::capturedLength==8);
    assert(j->tuner_run==0 && j->tuner_step==-1);
    destroy(j);
    j=mi.buildBmJob(42,1,256);
    assert(std::string(j->extranonce2)=="000000000000002A"); destroy(j);
    assert(!mi.buildBmJob(0,0,256,"FFFFFFFF"));
    assert(!mi.buildBmJob(0,0,256,"FFFFFFFFFFFFFFFG"));
    assert(!mi.buildBmJob(0,0,256,"FFFFFFFFFFFFFFFFFF"));
    assert(mi.getTunerProtocol()==2);
    mi.updateJob(&spec,assigned,2,4,0,8192);
    j=mi.buildBmJob(0,0,256,"00000090"); assert(j && std::string(j->extranonce2)=="00000090"); destroy(j);
    j=mi.buildBmJob(0,0,256,"FFFFFFFF"); assert(j); destroy(j);
    mi.updateJob(&spec,assigned,2,33,0,8192); assert(!mi.isValid());
    assert(!mi.buildBmJob(0,0,256,"FFFFFFFF"));
    MiningInfoV2Standard standard; assert(standard.getExtranonceBytes()==0);
    assert(standard.getTunerProtocol()==3);
    uint8_t merkle[32]{},prev[32]{};
    standard.updateJob(1,0x20000000,merkle,prev,123,0x17000001,0x1fffe000,8192);
    assert(standard.isValid());
    j=standard.buildBmJob(0xFFFFFFFF,0,512);
    assert(j && std::string(j->extranonce2).empty());
    assert(j->tuner_run==0 && j->tuner_step==-1 && !standard.isValid());
    destroy(j);
}
