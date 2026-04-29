#include <stdint.h>

// ── SHA1 ──────────────────────────────────────────────────────

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
        t = ROL32(a,5)+f+e+k+w[i];
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
        uint32_t sp = 64 - s->blen;
        uint32_t cp = n < sp ? n : sp;
        for (uint32_t i = 0; i < cp; i++) s->buf[s->blen+i] = d[i];
        s->blen += cp; d += cp; n -= cp;
        if (s->blen == 64) { sha1_block(s->h, s->buf); s->blen = 0; }
    }
}

static void sha1_done(sha1_t *s, uint8_t out[20]) {
    uint8_t pad[64];
    uint32_t p = s->blen;
    for (uint32_t i = 0; i < p; i++) pad[i] = s->buf[i];
    pad[p] = 0x80;
    for (uint32_t i = p+1; i < 64; i++) pad[i] = 0;
    if (p >= 56) { sha1_block(s->h, pad); for (int i=0;i<64;i++) pad[i]=0; }
    uint64_t bits = s->total * 8;
    pad[56]=(uint8_t)(bits>>56); pad[57]=(uint8_t)(bits>>48);
    pad[58]=(uint8_t)(bits>>40); pad[59]=(uint8_t)(bits>>32);
    pad[60]=(uint8_t)(bits>>24); pad[61]=(uint8_t)(bits>>16);
    pad[62]=(uint8_t)(bits>>8);  pad[63]=(uint8_t)bits;
    sha1_block(s->h, pad);
    for (int i = 0; i < 5; i++) {
        out[i*4]  =(uint8_t)(s->h[i]>>24); out[i*4+1]=(uint8_t)(s->h[i]>>16);
        out[i*4+2]=(uint8_t)(s->h[i]>>8);  out[i*4+3]=(uint8_t)s->h[i];
    }
}

// ── HMAC-SHA1 ─────────────────────────────────────────────────

static void hmac_sha1(const uint8_t *key, uint32_t klen,
                      const uint8_t *data, uint32_t dlen,
                      uint8_t out[20]) {
    uint8_t k[20], ipad[64], opad[64], ih[20];
    sha1_t s;
    if (klen > 64) {
        sha1_init(&s); sha1_feed(&s,key,klen); sha1_done(&s,k);
        key=k; klen=20;
    }
    for (int i = 0; i < 64; i++) {
        uint8_t ki = (uint32_t)i < klen ? key[i] : 0;
        ipad[i]=ki^0x36; opad[i]=ki^0x5C;
    }
    sha1_init(&s); sha1_feed(&s,ipad,64); sha1_feed(&s,data,dlen); sha1_done(&s,ih);
    sha1_init(&s); sha1_feed(&s,opad,64); sha1_feed(&s,ih,20);     sha1_done(&s,out);
}

// ── PBKDF2-HMAC-SHA1 (4096 iters, 32 bytes) ──────────────────

static void pbkdf2(const uint8_t *pw, uint32_t plen,
                   const uint8_t *salt, uint32_t slen,
                   uint8_t out[32]) {
    uint8_t sb[40], u[20], t[20];
    for (uint32_t blk = 1; blk <= 2; blk++) {
        for (uint32_t i = 0; i < slen; i++) sb[i] = salt[i];
        sb[slen]=0; sb[slen+1]=0; sb[slen+2]=0; sb[slen+3]=(uint8_t)blk;
        hmac_sha1(pw,plen,sb,slen+4,u);
        for (int j=0;j<20;j++) t[j]=u[j];
        for (int i=1;i<4096;i++) {
            hmac_sha1(pw,plen,u,20,u);
            for (int j=0;j<20;j++) t[j]^=u[j];
        }
        uint32_t oo=(blk-1)*20, ol=32-oo<20?32-oo:20;
        for (uint32_t j=0;j<ol;j++) out[oo+j]=t[j];
    }
}

// ── Shared buffers (fixed addresses in WASM linear memory) ────

static uint8_t g_pw[64];
static uint8_t g_ssid[33];
static uint8_t g_prf_data[76];
static uint8_t g_eapol[300];
static uint8_t g_mic[16];

// "Pairwise key expansion\0" — 23 bytes matching fast_prf512 sizeof(label)
static const uint8_t PRF_LABEL[23] = "Pairwise key expansion";

__attribute__((visibility("default"))) uint8_t* wasm_pw_buf()       { return g_pw; }
__attribute__((visibility("default"))) uint8_t* wasm_ssid_buf()     { return g_ssid; }
__attribute__((visibility("default"))) uint8_t* wasm_prf_data_buf() { return g_prf_data; }
__attribute__((visibility("default"))) uint8_t* wasm_eapol_buf()    { return g_eapol; }
__attribute__((visibility("default"))) uint8_t* wasm_mic_buf()      { return g_mic; }

// Returns 1 if password matches handshake, 0 otherwise
__attribute__((visibility("default")))
int wasm_try_password(uint32_t pwlen, uint32_t ssidlen, uint32_t eapollen) {
    // PBKDF2 → PMK
    uint8_t pmk[32];
    pbkdf2(g_pw, pwlen, g_ssid, ssidlen, pmk);

    // PRF-512 first block: label(23) + prf_data(76) + counter(1) = 100 bytes
    // Matches fast_prf512: sha1_update(label, sizeof(label)) + sha1_update(prf_data, 76) + sha1_update(&ctr, 1)
    uint8_t pin[100];
    for (int i = 0; i < 23; i++) pin[i]    = PRF_LABEL[i];
    for (int i = 0; i < 76; i++) pin[23+i] = g_prf_data[i];
    pin[99] = 0; // counter byte for block 0

    uint8_t kck20[20];
    hmac_sha1(pmk, 32, pin, 100, kck20); // kck20[0..15] = KCK

    // HMAC(KCK, eapol) → computed MIC
    uint8_t calc[20];
    hmac_sha1(kck20, 16, g_eapol, eapollen, calc);

    for (int i = 0; i < 16; i++) if (calc[i] != g_mic[i]) return 0;
    return 1;
}
