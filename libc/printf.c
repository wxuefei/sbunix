#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

char *strrev(char *str) {
    char *start = str, *end = str, temp;

    if(str == NULL)
        return str;
    while(*end)
        end++;
    end--;
    while(start < end) {
        temp = *end;
        *end-- = *start;
        *start++ = temp;
    }
    return str;
}

/**
* integer to ascii
*/
char *itoa(long long val, int base, char *str, size_t len) {
    int neg = 0;
    size_t i;
    if(str == NULL) {
        return NULL;
    }
    if(base < 2 || base > 36 || len < 1) {
        return str;
    }
    if(val == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }
    /* add negative sign if base-10 */
    if(val < 0 && base == 10) {
        neg = 1;
        val *= -1;
    }
    for(i = 0; i < len && val != 0; i++) {
        char c = (char)(val % base);
        if(c < 10) {
            c += '0';
        } else {
            c += 'a' - 10;
        }
        str[i] = c;
        val /= base;
    }
    if(neg) {
        str[i++] = '-';
    }
    str[(i < len)? i : len] = '\0';

    return strrev(str);
}

/**
* unsigned integer to ascii
*/
char *uitoa(unsigned long long val, int base, char *str, size_t len) {
    size_t i;
    if(str == NULL) {
        return NULL;
    }
    if(base < 2 || base > 36 || len < 1) {
        return str;
    }
    if(val == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }
    for(i = 0; i < len && val != 0; i++) {
        char c = (char)(val % base);
        if(c < 10) {
            c += '0';
        } else {
            c += 'a' - 10;
        }
        str[i] = c;
        val /= base;
    }
    str[i] = '\0';
    return strrev(str);
}

int parseint(char *str, int base) {
    int i = 0;

    if(str == NULL)
        return 0;
    while(*str) {
        i *= base;
        i += *str++ - '0';
    }
    return i;
}

int printf(const char *format, ...) {
	va_list ap;
	int printed = 0;

	va_start(ap, format);

    if(format == NULL)
        return 0;

	while(*format) {
        if(*format == '%') {
            int i;
            uint64_t ui64;
            int64_t i64;
            char arr[129] = {0}, *str, *towrite;
            int len;
            switch(format[1]) {
                case 'd':
                    i = va_arg(ap, int);
                    itoa((long long) i, 10, arr, sizeof(arr));
                    len = strlen(arr);
                    towrite = arr;
                    format += 2;
                    break;
                case 'l':
                    if(format[2] == 'l') {
                        i64 = va_arg(ap, int64_t);
                        itoa(i64, 10, arr, sizeof(arr));
                        len = strlen(arr);
                        towrite = arr;
                        format += 3;
                    } else {
                        i = va_arg(ap, int);
                        itoa((long long) i, 10, arr, sizeof(arr));
                        len = strlen(arr);
                        towrite = arr;
                        format += 2;
                    }
                    break;
                case 'p':
                    ui64 = va_arg(ap, uint64_t);
                    uitoa(ui64, 16, arr, sizeof(arr));
                    len = strlen(arr);
                    towrite = arr;
                    format += 2;
                    break;
                case 'o':
                    i = va_arg(ap, int);
                    uitoa(-1 & (uint64_t)i, 8, arr, sizeof(arr));
                    len = strlen(arr);
                    towrite = arr;
                    format += 2;
                    break;
                case 's':
                    str = va_arg(ap, char *);
                    len = strlen(str);
                    towrite = str;
                    format += 2;
                    break;
                case '%':
                    len = 1;
                    towrite = (char*)format;
                    format += 2;
                    break;
                default:
                    len = 1;
                    towrite = (char*)format;
                    ++printed;
                    ++format;
            }
            i = (int)write(STDOUT_FILENO, towrite, len);
            if(i < 0) {
                return -1;
            }
            printed += len;
        } else {
            write(STDOUT_FILENO, format, 1);
            ++printed;
            ++format;
        }
	}
    va_end(ap);
	return printed;
}
