/* Reference renderer for the debug-stream proof (TASK-1938).
 *
 * Reads jobs from stdin, one per line: the format string in hex, then the arguments, tab separated, each as
 * <type>:<value> with type d (signed), u (unsigned), f (double), c (char code), s (string in hex).
 * Writes the rendered text in hex, one line per job. Each conversion is rendered by the C library's snprintf
 * with its own flags, width and precision, which is what vsnprintf_P does on the device. Length modifiers
 * (l, h, z, ll) are replaced by l and the value is passed as long / unsigned long.
 *
 *   cc -O2 -o ref_printf ref_printf.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t unhex(const char *h, char *out, size_t cap) {
  size_t n = 0;
  while (h[0] && h[1] && n + 1 < cap) {
    unsigned v;
    sscanf(h, "%2x", &v);
    out[n++] = (char)v;
    h += 2;
  }
  out[n] = 0;
  return n;
}

int main(void) {
  static char line[1 << 16], fmt[4096], out[8192], piece[1024], spec[64], sval[1024];
  while (fgets(line, sizeof line, stdin)) {
    line[strcspn(line, "\r\n")] = 0;
    char *save = NULL, *tok = strtok_r(line, "\t", &save);
    if (!tok) continue;
    unhex(tok, fmt, sizeof fmt);
    size_t o = 0;
    for (const char *p = fmt; *p;) {
      if (o + 1 >= sizeof out) { fprintf(stderr, "output too long\n"); return 1; }
      if (*p != '%') { out[o++] = *p++; continue; }
      if (p[1] == '%') { out[o++] = '%'; p += 2; continue; }
      /* The spec handed to snprintf is rebuilt from a whitelist: '%', flags, at most 3 width digits,
         an optional '.' and at most 2 precision digits, 'l' for integers, one allowed conversion.
         Anything else (an unknown conversion such as %n, a long width, a '%' at the end) is refused. */
      const char *s = p + 1;
      size_t k = 0;
      spec[k++] = '%';
      while (*s && strchr("-+ #0", *s) && k < 8) spec[k++] = *s++;
      for (int d = 0; *s >= '0' && *s <= '9'; d++) { if (d == 3) goto bad_spec; spec[k++] = *s++; }
      if (*s == '.') {
        spec[k++] = *s++;
        for (int d = 0; *s >= '0' && *s <= '9'; d++) { if (d == 2) goto bad_spec; spec[k++] = *s++; }
      }
      while (*s && strchr("hlz", *s)) s++;
      char conv = *s;
      if (!conv || !strchr("diuxXcfsS", conv)) goto bad_spec;
      if (strchr("diuxX", conv)) spec[k++] = 'l';
      spec[k++] = (conv == 'S') ? 's' : conv;
      spec[k] = 0;
      char *arg = strtok_r(NULL, "\t", &save);
      if (!arg || !arg[0] || arg[1] != ':') { fprintf(stderr, "missing argument\n"); return 1; }
      const char *v = arg + 2;
      /* the argument type must match the conversion, so snprintf always gets the type it expects */
      const char *want = strchr("di", conv) ? "d" : strchr("uxX", conv) ? "u" : conv == 'f' ? "f"
                       : conv == 'c' ? "c" : "s";
      if (arg[0] != want[0]) { fprintf(stderr, "argument type %c does not fit %%%c\n", arg[0], conv); return 1; }
      switch (arg[0]) {
        case 'd': snprintf(piece, sizeof piece, spec, strtol(v, NULL, 10)); break;
        case 'u': snprintf(piece, sizeof piece, spec, strtoul(v, NULL, 10)); break;
        case 'f': snprintf(piece, sizeof piece, spec, strtod(v, NULL)); break;
        case 'c': snprintf(piece, sizeof piece, spec, (int)strtol(v, NULL, 10)); break;
        case 's': unhex(v, sval, sizeof sval); snprintf(piece, sizeof piece, spec, sval); break;
        default: fprintf(stderr, "bad type %c\n", arg[0]); return 1;
      }
      size_t pl = strlen(piece);
      if (o + pl >= sizeof out) { fprintf(stderr, "output too long\n"); return 1; }
      memcpy(out + o, piece, pl);
      o += pl;
      p = s + 1;
      continue;
    bad_spec:
      fprintf(stderr, "refused conversion spec in the format\n");
      return 1;
    }
    for (size_t i = 0; i < o; i++) printf("%02x", (unsigned char)out[i]);
    printf("\n");
  }
  return 0;
}
