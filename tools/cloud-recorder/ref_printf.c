/* Reference renderer for the debug-stream proof (TASK-1938).
 *
 * Reads jobs from stdin, one per line: the format string in hex, then the arguments, tab separated, each as
 * <type>:<value> with type d (signed), u (unsigned), f (double), c (char code), s (string in hex).
 * Writes the rendered text in hex, one line per job. Each conversion is rendered by the C library's snprintf,
 * which is what vsnprintf_P does on the device. Length modifiers (l, h, z, ll) are dropped and integers are
 * passed as long / unsigned long.
 *
 * snprintf only ever gets a format literal from render(): the flag ('', '0', '-' or '+', the ones the
 * firmware uses) and the conversion pick the literal; the width and precision go in as '*' arguments
 * (a negative precision means "not given", as in C). A spec outside that set is refused.
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

/* Renders one conversion into piece. Returns -1 when the flag/conversion pair is not in the table.
   Integers take a width only (the firmware gives them no precision); a precision on an integer is refused. */
static int render(char *piece, size_t cap, char flag, char conv, int w, int p, const char *v, char *sval, size_t scap) {
  long d = strtol(v, NULL, 10);
  unsigned long u = strtoul(v, NULL, 10);
  double f = strtod(v, NULL);
  if (strchr("diuxX", conv) && p >= 0) return -1;
  if (conv == 's' || conv == 'S') unhex(v, sval, scap);
  switch (flag) {
    case 0:
      switch (conv) {
        case 'd': case 'i': return snprintf(piece, cap, "%*ld", w, d);
        case 'u': return snprintf(piece, cap, "%*lu", w, u);
        case 'x': return snprintf(piece, cap, "%*lx", w, u);
        case 'X': return snprintf(piece, cap, "%*lX", w, u);
        case 'c': return snprintf(piece, cap, "%*c", w, (int)d);
        case 'f': return snprintf(piece, cap, "%*.*f", w, p, f);
        default:  return snprintf(piece, cap, "%*.*s", w, p, sval);
      }
    case '0':
      switch (conv) {
        case 'd': case 'i': return snprintf(piece, cap, "%0*ld", w, d);
        case 'u': return snprintf(piece, cap, "%0*lu", w, u);
        case 'x': return snprintf(piece, cap, "%0*lx", w, u);
        case 'X': return snprintf(piece, cap, "%0*lX", w, u);
        case 'f': return snprintf(piece, cap, "%0*.*f", w, p, f);
        default:  return -1;
      }
    case '-':
      switch (conv) {
        case 'd': case 'i': return snprintf(piece, cap, "%-*ld", w, d);
        case 'u': return snprintf(piece, cap, "%-*lu", w, u);
        case 'x': return snprintf(piece, cap, "%-*lx", w, u);
        case 'X': return snprintf(piece, cap, "%-*lX", w, u);
        case 'c': return snprintf(piece, cap, "%-*c", w, (int)d);
        case 'f': return snprintf(piece, cap, "%-*.*f", w, p, f);
        default:  return snprintf(piece, cap, "%-*.*s", w, p, sval);
      }
    case '+':
      switch (conv) {
        case 'd': case 'i': return snprintf(piece, cap, "%+*ld", w, d);
        case 'f': return snprintf(piece, cap, "%+*.*f", w, p, f);
        default:  return -1;
      }
  }
  return -1;
}

int main(void) {
  static char line[1 << 16], fmt[4096], out[8192], piece[1024], sval[1024];
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
      const char *s = p + 1;
      char flag = 0;
      if (*s && strchr("0-+", *s)) flag = *s++;
      int w = 0, prec = -1, digits = 0;
      while (*s >= '0' && *s <= '9') { if (++digits > 3) goto bad_spec; w = w * 10 + (*s++ - '0'); }
      if (*s == '.') {
        s++;
        prec = 0;
        digits = 0;
        while (*s >= '0' && *s <= '9') { if (++digits > 2) goto bad_spec; prec = prec * 10 + (*s++ - '0'); }
      }
      while (*s && strchr("hlz", *s)) s++;
      char conv = *s;
      if (!conv || !strchr("diuxXcfsS", conv)) goto bad_spec;
      char *arg = strtok_r(NULL, "\t", &save);
      if (!arg || !arg[0] || arg[1] != ':') { fprintf(stderr, "missing argument\n"); return 1; }
      /* the argument type must match the conversion */
      char want = strchr("di", conv) ? 'd' : strchr("uxX", conv) ? 'u' : conv == 'f' ? 'f' : conv == 'c' ? 'c' : 's';
      if (arg[0] != want) { fprintf(stderr, "argument type %c does not fit %%%c\n", arg[0], conv); return 1; }
      if (render(piece, sizeof piece, flag, conv, w, prec, arg + 2, sval, sizeof sval) < 0) goto bad_spec;
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
