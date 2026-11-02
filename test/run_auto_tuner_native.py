#!/usr/bin/env python3
"""Run the real tuner with in-memory NVS/clock, plus independent bigint checks."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
test = root / 'test/auto_tuner_native'
compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
if not compiler:
    raise SystemExit('C++17 compiler required')
with tempfile.TemporaryDirectory(prefix='extranonce-test-') as tmp:
    binary = str(Path(tmp) / 'tuner')
    subprocess.run([
        compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined',
        '-I' + str(test / 'stubs'), '-I' + str(root / 'main'),
        '-I' + str(root / 'components/arduinojson'),
        str(test / 'test_tuner.cpp'), str(root / 'main/auto_tuner.cpp'),
        '-o', binary,
    ], check=True)
    subprocess.run([binary], check=True)
    results = subprocess.check_output([binary, 'ranges'], text=True)
    checked = 0
    for line in results.splitlines():
        fields = line.split()
        if fields[0] in ('range','bits'):
            _, width, start, step, valid, low, high = fields
            width, start, step = int(width), int(start), int(step)
            total = 1 << (width if fields[0] == 'bits' else 8 * width)
            upper = start - step * 500
            expected_low = total * max(0, upper - 500) // 10000
            exclusive_high = total * upper // 10000
            assert bool(int(valid)) == (exclusive_high > expected_low), line
            if int(valid):
                assert int(low, 16) == expected_low, line
                assert int(high, 16) == exclusive_high - 1, line
            checked += 1
        elif fields[0] in ('manual','manualbits'):
            _, width, value = fields
            assert int(value, 16) == ((1 << (int(width) if fields[0] == 'manualbits' else 8 * int(width))) - 1) * 7324 // 10000, line
    print(f'PASS {checked} ranges checked against Python bigint; exact 73.24% at all widths 1..256 bits')

    builders = str(Path(tmp) / 'builders')
    subprocess.run([
        compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-Wno-unused-variable', '-include', 'pthread.h',
        '-fsanitize=address,undefined',
        '-I' + str(test / 'stubs'), '-I' + str(root / 'main/tasks'), '-I' + str(root / 'main'),
        '-I' + str(root / 'components/bm1397/include'),
        '-I' + str(root / 'components/stratum_v2/include'),
        str(test / 'test_builders.cpp'), str(root / 'main/tasks/mining_info_v2.cpp'),
        '-o', builders,
    ], check=True)
    subprocess.run([builders], check=True)
    print('PASS SV2 Extended builder: full 8-byte extranonce2 in coinbase and share metadata; normal/CAN fallback preserved')

    # Compile the actual V1 builder class with lightweight host transport/hash stubs.
    source = (root / 'main/tasks/create_jobs_task.cpp').read_text()
    builder_class = source[source.index('class MiningInfoV1'):source.index('// Global mining info instances')]
    v1_source = Path(tmp) / 'v1.cpp'
    v1_source.write_text(r'''#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include "create_jobs_task.h"
#include "extranonce_range.h"
#include "hash_capture.h"
#define MALLOC malloc
#define CALLOC calloc
#define safe_free(p) do { free(p); p=nullptr; } while(0)
MiningInfoBase::~MiningInfoBase() {}
std::string assembledCoinbase;
void calculate_merkle_root_hash(const char *coinbase, const uint8_t (*)[32], int, char *out) {
    assembledCoinbase=coinbase; memset(out,'0',64); out[64]=0;
}
void construct_bm_job(mining_notify *, const char *, uint32_t, bm_job *) {}
''' + builder_class + r'''
void destroy(bm_job *j) { free(j->jobid); free(j->extranonce2); free(j); }
int main() {
    MiningInfoV1 mi;
    mining_notify notify{}; notify.job_id=(char *)"7"; notify.ntime=123;
    notify.coinbase_1=(char *)"0102"; notify.coinbase_2=(char *)"AA";
    mi.create_job_mining_notify(&notify); mi.set_enonce((char *)"BEEF",8);
    bm_job *j=mi.buildBmJob(0,0,256,"FFFFFFFFFFFFFFFE");
    assert(std::string(j->extranonce2)=="FFFFFFFFFFFFFFFE");
    assert(assembledCoinbase=="0102BEEFFFFFFFFFFFFFFFFEAA");
    assert(j->capture_inputs_id==42 && HashCapture::capturedOffset==4 && HashCapture::capturedLength==8);
    assert((HashCapture::capturedCoinbase==std::vector<uint8_t>{1,2,0xBE,0xEF,255,255,255,255,255,255,255,254,0xAA}));
    assert(j->tuner_run==0 && j->tuner_step==-1); destroy(j);
    mi.set_enonce((char *)"BEEF",4);
    assert(!mi.buildBmJob(0,0,256,"FFFFFFFFFFFFFFFF"));
    assert(!mi.buildBmJob(0,0,256,"FFFFFFFFF"));
    assert(!mi.buildBmJob(0,0,256,"TLEF0000"));
    assert(!mi.buildBmJob(0,0,256,"0xFFFFFFFF"));
    j=mi.buildBmJob(0,0,256,"0000008F"); assert(j && std::string(j->extranonce2)=="0000008F"); destroy(j);
    j=mi.buildBmJob(0,0,256,"FFFFFFFF"); assert(j); destroy(j);
    j=mi.buildBmJob(0,0,256,"00000090"); assert(j); destroy(j);
    assert(mi.getTunerProtocol()==1);
    mi.set_enonce((char *)"BEEF",1);
    j=mi.buildBmJob(0xFFFFFFFF,0,256); assert(std::string(j->extranonce2)=="ff"); destroy(j);
}
''')
    v1_binary = str(Path(tmp) / 'v1')
    subprocess.run([
        compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined',
        '-I' + str(test / 'stubs'), '-I' + str(root / 'main/tasks'), '-I' + str(root / 'main'),
        '-I' + str(root / 'components/bm1397/include'),
        str(v1_source), '-o', v1_binary,
    ], check=True)
    subprocess.run([v1_binary], check=True)
    print('PASS V1 builder: full 8-byte extranonce2 in coinbase, exact pool width, metadata')

    config_header=(root / 'main/stratum/stratum_config.h').read_text()
    config_header=config_header[config_header.index('enum StratumProtocol'):]
    config_source=(root / 'main/stratum/stratum_config.cpp').read_text()
    config_source=config_source[config_source.index('static bool strEq'):config_source.index('/*')]
    getters = []
    for index, names in enumerate([
        ['getStratumURL','getStratumPortNumber','getStratumUser','getStratumPass','isStratumEnonceSubscribe','isStratumTLS','getStratumProtocol','getSV2ChannelType','isSV2RequireAuth','getSV2AuthorityPubkey'],
        ['getStratumFallbackURL','getStratumFallbackPortNumber','getStratumFallbackUser','getStratumFallbackPass','isStratumFallbackEnonceSubscribe','isStratumFallbackTLS','getFallbackStratumProtocol','getFallbackSV2ChannelType','isFallbackSV2RequireAuth','getFallbackSV2AuthorityPubkey'],
    ]):
        for name,field,kind in zip(names,['host','port','user','password','ensub','tls','protocol','channel','auth','key'],['string','int','string','string','bool','bool','int','int','bool','string']):
            value=f'values[{index}].{field}'
            getters.append(f'char *{name}() {{ return strdup({value}.c_str()); }}' if kind=='string' else f'{kind} {name}() {{ return {value}; }}')
    reconnect=Path(tmp) / 'reconnect.cpp'
    reconnect.write_text(r'''#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <cassert>
#define safe_free(p) do { free(p); p=nullptr; } while(0)
namespace Config {
struct Values {
 std::string host="pool.example",user="worker",password="x",key="public-key";
 int port=3333,protocol=1,channel=0; bool ensub=false,tls=false,auth=true;
};
Values values[2];
'''+'\n'.join(getters)+'\n}\n'+config_header+'\n'+config_source+r'''
int main() {
 StratumConfig primary(0),secondary(1);
 assert(!primary.reload() && !secondary.reload());
 Config::values[0].channel=1;
 assert(primary.reload()); assert(!primary.reload()); assert(!secondary.reload());
 Config::values[0].key="new-public-key";
 assert(primary.reload() && !primary.reload());
 Config::values[0].auth=false;
 assert(primary.reload() && !primary.reload());
 Config::values[1].channel=1;
 assert(secondary.reload() && !secondary.reload());
 primary.copyInto(&secondary); assert(!secondary.reload());
}
''')
    reconnect_binary=str(Path(tmp) / 'reconnect')
    subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(reconnect),'-o',reconnect_binary],check=True)
    subprocess.run([reconnect_binary],check=True)
    print('PASS channel reconnect: V2 Standard/Extended switch, authority key, auth, per-pool isolation')
