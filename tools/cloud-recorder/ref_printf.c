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
      if (*p != '%') { out[o++] = *p++; continue; }
      if (p[1] == '%') { out[o++] = '%'; p += 2; continue; }
      const char *s = p + 1;
      while (strchr("-+ #0", *s)) s++;
      while (*s >= '0' && *s <= '9') s++;
      if (*s == '.') { s++; while (*s >= '0' && *s <= '9') s++; }
      size_t head = (size_t)(s - p);
      while (strchr("hlz", *s)) s++;
      char conv = *s;
      memcpy(spec, p, head);
      size_t k = head;
      if (strchr("diuxXc", conv) && conv != 'c') spec[k++] = 'l';
      spec[k++] = (conv == 'S') ? 's' : conv;
      spec[k] = 0;
      char *arg = strtok_r(NULL, "\t", &save);
      if (!arg) { fprintf(stderr, "missing argument\n"); return 1; }
      const char *v = arg + 2;
      switch (arg[0]) {
        case 'd': snprintf(piece, sizeof piece, spec, strtol(v, NULL, 10)); break;
        case 'u': snprintf(piece, sizeof piece, spec, strtoul(v, NULL, 10)); break;
        case 'f': snprintf(piece, sizeof piece, spec, strtod(v, NULL)); break;
        case 'c': snprintf(piece, sizeof piece, spec, (int)strtol(v, NULL, 10)); break;
        case 's': unhex(v, sval, sizeof sval); snprintf(piece, sizeof piece, spec, sval); break;
        default: fprintf(stderr, "bad type %c\n", arg[0]); return 1;
      }
      size_t pl = strlen(piece);
      memcpy(out + o, piece, pl);
      o += pl;
      p = s + 1;
    }
    for (size_t i = 0; i < o; i++) printf("%02x", (unsigned char)out[i]);
    printf("\n");
  }
  return 0;
}
