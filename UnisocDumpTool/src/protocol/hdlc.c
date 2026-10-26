#include "hdlc.h"

size_t hdlc_encode(const uint8_t *payload, size_t len, uint8_t *out, size_t outcap)
{
    size_t o = 0;
    size_t i;

    /* Worst case every byte escapes to two, plus two delimiters. */
    if (outcap < 2) return 0;

    out[o++] = HDLC_FLAG;
    for (i = 0; i < len; i++) {
        uint8_t b = payload[i];
        if (b == HDLC_FLAG || b == HDLC_ESC) {
            if (o + 2 >= outcap) return 0;
            out[o++] = HDLC_ESC;
            out[o++] = (uint8_t)(b ^ HDLC_ESC_XOR);
        } else {
            if (o + 1 >= outcap) return 0;
            out[o++] = b;
        }
    }
    if (o + 1 > outcap) return 0;
    out[o++] = HDLC_FLAG;
    return o;
}

size_t hdlc_decode(const uint8_t *in, size_t len, uint8_t *out, size_t outcap)
{
    size_t o = 0;
    size_t i;

    for (i = 0; i < len; i++) {
        uint8_t b = in[i];
        if (b == HDLC_ESC) {
            if (i + 1 >= len) return (size_t)-1;    /* dangling escape */
            i++;
            b = (uint8_t)(in[i] ^ HDLC_ESC_XOR);
        } else if (b == HDLC_FLAG) {
            /* A delimiter inside the body is malformed; caller should have
             * stripped leading/trailing flags already. */
            return (size_t)-1;
        }
        if (o >= outcap) return (size_t)-1;
        out[o++] = b;
    }
    return o;
}
