#include <stdint.h>
#include <wasm_simd128.h>

/* ── Scalar SHA-1 ───────────────────────────────────────────── */

#define ROL32(x,n) (((x)<<(n))|((x)>>(32-(n))))

typedef struct {
    uint32_t h[5];
    uint8_t  buf[64];
    uint32_t blen;
    uint64_t total;
} sha1_t;

static void sha1_block(uint32_t h[5], const uint8_t b[64]) {
    uint32_t w[80], t;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)b[i*4]<<24)|((uint32_t)b[i*4+1]<<16)|
               ((uint32_t)b[i*4+2]<<8)|(uint32_t)b[i*4+3];
    for (int i = 16; i < 80; i++) { t=w[i-3]^w[i-8]^w[i-14]^w[i-16]; w[i]=ROL32(t,1); }
    uint32_t a=h[0],b2=h[1],c=h[2],d=h[3],e=h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if      (i<20){f=(b2&c)|(~b2&d);      k=0x5A827999u;}
        else if (i<40){f=b2^c^d;              k=0x6ED9EBA1u;}
        else if (i<60){f=(b2&c)|(b2&d)|(c&d);k=0x8F1BBCDCu;}
        else          {f=b2^c^d;              k=0xCA62C1D6u;}
        t=ROL32(a,5)+f+e+k+w[i];
        e=d; d=c; c=ROL32(b2,30); b2=a; a=t;
    }
    h[0]+=a; h[1]+=b2; h[2]+=c; h[3]+=d; h[4]+=e;
}

static void sha1_init(sha1_t *s) {
    s->h[0]=0x67452301u; s->h[1]=0xEFCDAB89u;
    s->h[2]=0x98BADCFEu; s->h[3]=0x10325476u; s->h[4]=0xC3D2E1F0u;
    s->blen=0; s->total=0;
}

static void sha1_feed(sha1_t *s, const uint8_t *d, uint32_t n) {
    s->total += n;
    while (n > 0) {
        uint32_t sp=64-s->blen, cp=n<sp?n:sp;
        for (uint32_t i=0; i<cp; i++) s->buf[s->blen+i]=d[i];
        s->blen+=cp; d+=cp; n-=cp;
        if (s->blen==64) { sha1_block(s->h,s->buf); s->blen=0; }
    }
}

static void sha1_done(sha1_t *s, uint8_t out[20]) {
    uint8_t pad[64];
    uint32_t p=s->blen;
    for (uint32_t i=0; i<p; i++) pad[i]=s->buf[i];
    pad[p]=0x80;
    for (uint32_t i=p+1; i<64; i++) pad[i]=0;
    if (p>=56) { sha1_block(s->h,pad); for (int i=0;i<64;i++) pad[i]=0; }
    uint64_t bits=s->total*8;
    pad[56]=(uint8_t)(bits>>56); pad[57]=(uint8_t)(bits>>48);
    pad[58]=(uint8_t)(bits>>40); pad[59]=(uint8_t)(bits>>32);
    pad[60]=(uint8_t)(bits>>24); pad[61]=(uint8_t)(bits>>16);
    pad[62]=(uint8_t)(bits>>8);  pad[63]=(uint8_t)bits;
    sha1_block(s->h,pad);
    for (int i=0; i<5; i++) {
        out[i*4]  =(uint8_t)(s->h[i]>>24); out[i*4+1]=(uint8_t)(s->h[i]>>16);
        out[i*4+2]=(uint8_t)(s->h[i]>>8);  out[i*4+3]=(uint8_t)s->h[i];
    }
}

/* ── Pre-computed HMAC-SHA1 ─────────────────────────────────── */

typedef struct { uint32_t h[5]; } sha1_mid_t;

static sha1_mid_t sha1_precompute_block(const uint8_t block[64]) {
    sha1_mid_t m;
    m.h[0]=0x67452301u; m.h[1]=0xEFCDAB89u;
    m.h[2]=0x98BADCFEu; m.h[3]=0x10325476u; m.h[4]=0xC3D2E1F0u;
    sha1_block(m.h, block);
    return m;
}

static void sha1_finish_from(const sha1_mid_t *pre, uint64_t pre_bytes,
                              const uint8_t *data, uint32_t dlen, uint8_t out[20]) {
    sha1_t s;
    s.h[0]=pre->h[0]; s.h[1]=pre->h[1]; s.h[2]=pre->h[2];
    s.h[3]=pre->h[3]; s.h[4]=pre->h[4];
    s.blen=0; s.total=pre_bytes;
    sha1_feed(&s,data,dlen); sha1_done(&s,out);
}

static void hmac_sha1_fast(const sha1_mid_t *is, const sha1_mid_t *os,
                            const uint8_t *data, uint32_t dlen, uint8_t out[20]) {
    uint8_t ih[20];
    sha1_finish_from(is,64,data,dlen,ih);
    sha1_finish_from(os,64,ih,20,out);
}

static void hmac_sha1(const uint8_t *key, uint32_t klen,
                      const uint8_t *data, uint32_t dlen, uint8_t out[20]) {
    uint8_t k[20], ipad[64], opad[64], ih[20];
    sha1_t s;
    if (klen>64) { sha1_init(&s); sha1_feed(&s,key,klen); sha1_done(&s,k); key=k; klen=20; }
    for (int i=0; i<64; i++) {
        uint8_t ki=(uint32_t)i<klen?key[i]:0;
        ipad[i]=ki^0x36; opad[i]=ki^0x5C;
    }
    sha1_init(&s); sha1_feed(&s,ipad,64); sha1_feed(&s,data,dlen); sha1_done(&s,ih);
    sha1_init(&s); sha1_feed(&s,opad,64); sha1_feed(&s,ih,20);     sha1_done(&s,out);
}

/* ── 4-wide SIMD SHA-1 ──────────────────────────────────────── */
/* Each v128_t lane corresponds to one of 4 passwords processed in parallel. */

#define VROL32(v,n) wasm_v128_or(wasm_i32x4_shl((v),(n)), wasm_u32x4_shr((v),(uint32_t)(32-(n))))

static void sha1_block_4wide(v128_t h[5], const v128_t w_in[16]) {
    v128_t W[80];
    for (int i = 0;  i < 16; i++) W[i] = w_in[i];
    for (int i = 16; i < 80; i++) {
        v128_t st = wasm_v128_xor(wasm_v128_xor(W[i-3],W[i-8]),
                                   wasm_v128_xor(W[i-14],W[i-16]));
        W[i] = VROL32(st,1);
    }
    v128_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4];
#define VK(x)      wasm_i32x4_splat(x)
#define VF1(b,c,d) wasm_v128_or(wasm_v128_and(b,c),wasm_v128_and(wasm_v128_not(b),d))
#define VF2(b,c,d) wasm_v128_xor(wasm_v128_xor(b,c),d)
#define VF3(b,c,d) wasm_v128_or(wasm_v128_or(wasm_v128_and(b,c),wasm_v128_and(b,d)),wasm_v128_and(c,d))
#define VR(f,k,wi) do { \
    v128_t _vt = wasm_i32x4_add(wasm_i32x4_add(wasm_i32x4_add(VROL32(a,5),(f)),e), \
                                 wasm_i32x4_add(VK(k),(wi))); \
    e=d; d=c; c=VROL32(b,30); b=a; a=_vt; \
} while(0)
    for (int i =  0; i < 20; i++) { VR(VF1(b,c,d),0x5A827999u,W[i]); }
    for (int i = 20; i < 40; i++) { VR(VF2(b,c,d),0x6ED9EBA1u,W[i]); }
    for (int i = 40; i < 60; i++) { VR(VF3(b,c,d),0x8F1BBCDCu,W[i]); }
    for (int i = 60; i < 80; i++) { VR(VF2(b,c,d),0xCA62C1D6u,W[i]); }
#undef VR
#undef VF3
#undef VF2
#undef VF1
#undef VK
    h[0]=wasm_i32x4_add(h[0],a); h[1]=wasm_i32x4_add(h[1],b);
    h[2]=wasm_i32x4_add(h[2],c); h[3]=wasm_i32x4_add(h[3],d); h[4]=wasm_i32x4_add(h[4],e);
}

/* HMAC-SHA1 for exactly 20-byte data, 4 passwords in parallel.
   After the 64-byte ipad/opad pre-state, feeding 20 bytes + padding = 1 SHA1 block.
   Total bits = (64+20)*8 = 672 = 0x2A0. Padding block:
     [u[0..19]] [0x80] [zeros x35] [0x000002A0]
   = 5 BE32 words from u, then 0x80000000, 9 zeros, 0x000002A0 */
static void hmac_20_4wide(const v128_t is[5], const v128_t os[5],
                           v128_t u[5], v128_t t[5]) {
    v128_t w[16];
    w[0]=u[0]; w[1]=u[1]; w[2]=u[2]; w[3]=u[3]; w[4]=u[4];
    w[5]=wasm_i32x4_splat(0x80000000u);
    v128_t z=wasm_i32x4_splat(0);
    w[6]=z; w[7]=z; w[8]=z; w[9]=z; w[10]=z; w[11]=z; w[12]=z; w[13]=z; w[14]=z;
    w[15]=wasm_i32x4_splat(0x000002A0u);

    v128_t h[5];
    h[0]=is[0]; h[1]=is[1]; h[2]=is[2]; h[3]=is[3]; h[4]=is[4];
    sha1_block_4wide(h, w);

    w[0]=h[0]; w[1]=h[1]; w[2]=h[2]; w[3]=h[3]; w[4]=h[4];
    /* w[5..15] unchanged (same padding for outer hash) */
    h[0]=os[0]; h[1]=os[1]; h[2]=os[2]; h[3]=os[3]; h[4]=os[4];
    sha1_block_4wide(h, w);

    for (int j=0; j<5; j++) { u[j]=h[j]; t[j]=wasm_v128_xor(t[j],h[j]); }
}

/* PBKDF2-HMAC-SHA1 for 4 passwords in parallel, 4096 iterations, 32-byte output.
   First iteration per PBKDF2 block uses scalar code (variable-length salt input).
   Remaining 4095 iterations use 4-wide SIMD. */
static void pbkdf2_4wide(const uint8_t pw[4][64], const uint32_t pl[4],
                          const uint8_t *salt, uint32_t slen,
                          uint8_t out[4][32]) {
    sha1_mid_t istate[4], ostate[4];
    for (int p=0; p<4; p++) {
        uint8_t ipad[64], opad[64];
        for (int i=0; i<64; i++) {
            uint8_t ki=(uint32_t)i<pl[p]?pw[p][i]:0;
            ipad[i]=ki^0x36; opad[i]=ki^0x5C;
        }
        istate[p]=sha1_precompute_block(ipad);
        ostate[p]=sha1_precompute_block(opad);
    }

    v128_t is4[5], os4[5];
    for (int j=0; j<5; j++) {
        is4[j]=wasm_i32x4_make((int32_t)istate[0].h[j],(int32_t)istate[1].h[j],
                                (int32_t)istate[2].h[j],(int32_t)istate[3].h[j]);
        os4[j]=wasm_i32x4_make((int32_t)ostate[0].h[j],(int32_t)ostate[1].h[j],
                                (int32_t)ostate[2].h[j],(int32_t)ostate[3].h[j]);
    }

    for (uint32_t blk=1; blk<=2; blk++) {
        uint8_t sb[40];
        for (uint32_t i=0; i<slen; i++) sb[i]=salt[i];
        sb[slen]=0; sb[slen+1]=0; sb[slen+2]=0; sb[slen+3]=(uint8_t)blk;

        uint8_t u_sc[4][20];
        for (int p=0; p<4; p++)
            hmac_sha1_fast(&istate[p],&ostate[p],sb,slen+4,u_sc[p]);

#define BE32(p,j) (((uint32_t)u_sc[p][j*4]<<24)|((uint32_t)u_sc[p][j*4+1]<<16)| \
                   ((uint32_t)u_sc[p][j*4+2]<<8)|(uint32_t)u_sc[p][j*4+3])
        v128_t u4[5], t4[5];
        for (int j=0; j<5; j++) {
            u4[j]=wasm_i32x4_make((int32_t)BE32(0,j),(int32_t)BE32(1,j),
                                   (int32_t)BE32(2,j),(int32_t)BE32(3,j));
            t4[j]=u4[j];
        }
#undef BE32

        for (int iter=1; iter<4096; iter++)
            hmac_20_4wide(is4,os4,u4,t4);

        /* Extract bytes from 4-wide result into per-password output buffers.
           blk=1: oo=0,  ol=20 (5 complete words)
           blk=2: oo=20, ol=12 (3 complete words, PMK = 32 bytes total) */
        uint32_t oo=(blk-1)*20;
        uint32_t words=(oo+20>32)?(32-oo)/4:5;
        uint32_t tmp[4] __attribute__((aligned(16)));
        for (int p=0; p<4; p++) {
            for (uint32_t j=0; j<words; j++) {
                wasm_v128_store(tmp,t4[j]);
                uint32_t w=tmp[p];
                out[p][oo+j*4+0]=(uint8_t)(w>>24);
                out[p][oo+j*4+1]=(uint8_t)(w>>16);
                out[p][oo+j*4+2]=(uint8_t)(w>>8);
                out[p][oo+j*4+3]=(uint8_t)(w);
            }
        }
    }
}

/* ── Shared buffers ─────────────────────────────────────────── */

static uint8_t g_pw[4][64];
static uint8_t g_ssid[33];
static uint8_t g_prf_data[76];
static uint8_t g_eapol[300];
static uint8_t g_mic[16];

static const uint8_t PRF_LABEL[23] = "Pairwise key expansion";

__attribute__((visibility("default"))) uint8_t* wasm_pw0_buf()      { return g_pw[0]; }
__attribute__((visibility("default"))) uint8_t* wasm_pw1_buf()      { return g_pw[1]; }
__attribute__((visibility("default"))) uint8_t* wasm_pw2_buf()      { return g_pw[2]; }
__attribute__((visibility("default"))) uint8_t* wasm_pw3_buf()      { return g_pw[3]; }
__attribute__((visibility("default"))) uint8_t* wasm_ssid_buf()     { return g_ssid; }
__attribute__((visibility("default"))) uint8_t* wasm_prf_data_buf() { return g_prf_data; }
__attribute__((visibility("default"))) uint8_t* wasm_eapol_buf()    { return g_eapol; }
__attribute__((visibility("default"))) uint8_t* wasm_mic_buf()      { return g_mic; }

/* Try up to 4 passwords in parallel using SIMD PBKDF2.
   count: number of valid passwords (1-4); unused slots are padded with pw[0].
   Returns the index (0-3) of the matching password, or -1. */
__attribute__((visibility("default")))
int wasm_try_passwords_batch(uint32_t count,
                              uint32_t plen0, uint32_t plen1, uint32_t plen2, uint32_t plen3,
                              uint32_t ssidlen, uint32_t eapollen) {
    if (count==0) return -1;
    uint32_t pl[4]={plen0,plen1,plen2,plen3};
    for (uint32_t p=count; p<4; p++) {
        for (int i=0; i<64; i++) g_pw[p][i]=g_pw[0][i];
        pl[p]=plen0;
    }

    uint8_t pmk[4][32];
    pbkdf2_4wide((const uint8_t (*)[64])g_pw, pl, g_ssid, ssidlen, pmk);

    uint8_t pin[100];
    for (int i=0; i<23; i++) pin[i]    =PRF_LABEL[i];
    for (int i=0; i<76; i++) pin[23+i] =g_prf_data[i];
    pin[99]=0;

    for (uint32_t p=0; p<count; p++) {
        uint8_t kck20[20], calc[20];
        hmac_sha1(pmk[p],32,pin,100,kck20);
        hmac_sha1(kck20,16,g_eapol,eapollen,calc);
        int match=1;
        for (int i=0; i<16; i++) if (calc[i]!=g_mic[i]) { match=0; break; }
        if (match) return (int)p;
    }
    return -1;
}
