// Author: Dawie Boers

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>

intmax_t max(intmax_t lhs, intmax_t rhs)
{
    return (rhs > lhs) ? rhs : lhs;
}

intmax_t min(intmax_t lhs, intmax_t rhs)
{
    return (rhs < lhs) ? rhs : lhs;
}

typedef struct
{
    uint8_t *compressed;
    size_t length;
} CompressedData;

// Change to uint16_t for input greater than 255 characters
#define DIST_TYPE uint8_t
#define LENG_TYPE uint8_t

#define MAX_MATCH UINT8_MAX
// Exclude matches too small to save memory on.
#define MIN_MATCH sizeof(DIST_TYPE) + sizeof(LENG_TYPE)

void mprintf(char *str)
{
    size_t len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        unsigned char c = str[i];
        if (c >= '!' && c <= '~')
        {
            putchar(c);
        }
        else if (c == '\n' || c == ' ')
        {
            putchar(c);
        }
        else
        {
            printf("%d", c);
        }
    }
}

CompressedData lz77(char *input, const size_t input_length)
{
    uint8_t *out = malloc(input_length + 1);
    uint8_t *outptr = out;

    if (out == NULL)
    {
        return (CompressedData){NULL, -1};
    }

    int max_window_size = min(2000, input_length);
    int window_size = 0;
    char *window = input;
    char *cursor = input;

    out[0] = 0;
    uint8_t *buffer = out;
    outptr++;
    int bit_count = 0;

    while (cursor - input < input_length)
    {
        window_size = min(max_window_size, cursor - input);

        int i_max = min(min(window_size, MAX_MATCH), input_length - (cursor - input) - 1);
        int i = i_max;
        char *match = NULL;
        bool is_match = false;
        for (; i >= MIN_MATCH; i--)
        {
            char front_of_input[i + 1];
            strncpy(front_of_input, cursor, i);
            front_of_input[i] = '\0';
            match = strstr(window, front_of_input);
            if (match != NULL && match < cursor)
            {
                is_match = true;
                break;
            }
        }

        // Ensure match length doesn't extend beyond what
        // would be available in output during decompression
        if (is_match)
        {
            int max_safe_match_length = cursor - match;
            if (i > max_safe_match_length)
            {
                i = max_safe_match_length;
            }
        }

        // Bit flags
        // Uses the LZSS system with flag buffers to encode stuff

        *buffer = (*buffer << 1) | is_match;
        bit_count++;

        // Write

        DIST_TYPE d;
        LENG_TYPE l;
        char c;
        if (is_match)
        {
            d = match - input;
            l = i;
            c = cursor[l];

            memcpy(outptr, &d, sizeof(DIST_TYPE));
            outptr += sizeof(DIST_TYPE);
            memcpy(outptr, &l, sizeof(LENG_TYPE));
            outptr += sizeof(LENG_TYPE);
        }
        else
        {
            d = 0;
            l = 0;
            c = *cursor;
        }

        *outptr++ = c;

        if (bit_count == 8)
        {
            *outptr = 0;
            buffer = outptr;
            outptr++;
            bit_count = 0;
        }

        // Update window and cursor

        cursor += l + 1;
        if (cursor - window >= max_window_size)
        {
            window += l + 1;
        }
    }

    if (bit_count > 0)
    {
        *buffer <<= (8 - bit_count);
    }

    *outptr = '\0';

    return (CompressedData){out, outptr - out};
}

char *decompress(uint8_t *comp, const size_t comp_length)
{
    size_t output_size = 0;
    uint8_t *cursor = comp;

    while (cursor - comp < comp_length)
    {
        uint8_t buffer = *cursor++;

        for (int i = 7; i >= 0; i--)
        {
            if (cursor - comp >= comp_length)
            {
                break;
            }

            if ((buffer >> i) & 1)
            {
                DIST_TYPE d;
                LENG_TYPE l;

                memcpy(&d, cursor, sizeof(DIST_TYPE));
                cursor += sizeof(DIST_TYPE);
                memcpy(&l, cursor, sizeof(LENG_TYPE));
                cursor += sizeof(LENG_TYPE);
                output_size += l;
            }

            output_size++;
            cursor++;
        }
    }

    cursor = comp;
    char *out = malloc(output_size + 1);
    out[output_size] = '\0'; // For safety
    char *outptr = out;

    while (cursor - comp < comp_length && outptr - out < output_size)
    {
        uint8_t buffer = *cursor++;

        for (int i = 7; i >= 0; i--)
        {
            if (cursor - comp >= comp_length || outptr - out >= output_size)
            {
                break;
            }

            char c;
            bool ismtc = (buffer >> i) & 1;
            if (ismtc)
            {
                DIST_TYPE d;
                LENG_TYPE l;

                memcpy(&d, cursor, sizeof(DIST_TYPE));
                cursor += sizeof(DIST_TYPE);
                memcpy(&l, cursor, sizeof(LENG_TYPE));
                cursor += sizeof(LENG_TYPE);

                memmove(outptr, out + d, l);
                outptr += l;
            }

            c = *cursor++;

            *outptr++ = c;
        }
    }

    *outptr = '\0';

    return out;
}

/**
 * Here's what "Never ever find me." looks like compressed:
 *
 * 0x00       00000010  Flag byte: 6 literal characters, 1 match, then 1 more literal.
 * 0x01-0x06  Never_     6 literals (_ represents space).
 * 0x07       1          Unsigned 8-bit number; match pointer.
 * 0x08       5          Unsigned 8-bit number; match length.
 * 0x09       f          Trailing literal character (from match).
 * 0x0a       00000000  Flag byte: 8 literal characters.
 * 0x0b-0x12  ind_me.|   8 literals (| represents null terminator).
 */

int main()
{
    char *input;

    FILE *fp = fopen("input.txt", "r");
    fseek(fp, 0, SEEK_END);
    size_t input_size = ftell(fp);
    rewind(fp);
    input = malloc(input_size + 1);
    fread(input, 1, input_size, fp);
    input[input_size] = '\0';

    CompressedData compressed = lz77(input, strlen(input));

    int comp_chars = min(50, compressed.length);
    uint8_t compressed_abridged[comp_chars + 1];
    memcpy(compressed_abridged, compressed.compressed, comp_chars);

    char *decompressed = decompress(compressed.compressed, compressed.length);

    printf("Decompressed: (");
    mprintf(decompressed);
    printf(")\n");

    if (strcmp(input, decompressed) == 0)
    {
        printf("Correctly compressed and decompressed!\n");
    }
    else
    {
        printf("Something went wrong; strings are not the same.\n");
    }

    double ratio = compressed.length / (double)strlen(input);

    printf("Compression ratio: %f\n", ratio);

    free(input);
    free(compressed.compressed);
    free(decompressed);
}