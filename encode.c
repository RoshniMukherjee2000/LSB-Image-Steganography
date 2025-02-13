#include <stdio.h>
#include "encode.h"
#include "types.h"
#include <string.h>
#include "common.h"

/* Function definition for check operation type */
OperationType check_operation_type(char *argv[])
{
    if (strcmp(argv[1], "-e") == 0)
        return e_encode;
    if (strcmp(argv[1], "-d") == 0)
        return e_decode;
    else
        return e_unsupported;
}

/* Get image size (width * height * bytes per pixel) */
uint get_image_size_for_bmp(FILE *fptr_image)
{
    uint width, height;
    // Seek to 18th byte to get width and height
    fseek(fptr_image, 18, SEEK_SET);
    fread(&width, sizeof(int), 1, fptr_image);
    fread(&height, sizeof(int), 1, fptr_image);

    printf("Image width: %u, height: %u\n", width, height);
    
    // Return the image size (3 bytes per pixel for RGB)
    return width * height * 3;
}

/* Get File pointers for source, secret, and stego images */
Status open_files(EncodeInfo *encInfo)
{
    encInfo->fptr_src_image = fopen(encInfo->src_image_fname, "rb");
    if (encInfo->fptr_src_image == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->src_image_fname);
        return e_failure;
    }

    encInfo->fptr_secret = fopen(encInfo->secret_fname, "r");
    if (encInfo->fptr_secret == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->secret_fname);
        return e_failure;
    }

    encInfo->fptr_stego_image = fopen(encInfo->stego_image_fname, "wb");
    if (encInfo->fptr_stego_image == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->stego_image_fname);
        return e_failure;
    }

    return e_success;
}

/* Validate and read encode arguments */
Status read_and_validate_encode_args(char *argv[], EncodeInfo *encInfo)
{
    if (strcmp(strstr(argv[2], "."), ".bmp") == 0)
        encInfo->src_image_fname = argv[2];
    else
        return e_failure;

    if (strcmp(strstr(argv[3], "."), ".txt") == 0)
        encInfo->secret_fname = argv[3];
    else
        return e_failure;

    encInfo->stego_image_fname = argv[4] ? argv[4] : "stego.bmp";
    return e_success;
}

Status encode_magic_string(char *magic_string, EncodeInfo *encInfo)
{
    encode_data_to_image(magic_string, 2, encInfo->fptr_src_image, encInfo->fptr_stego_image, encInfo);
    return e_success;
}

/* Check if image can hold the secret data */
Status check_capacity(EncodeInfo *encInfo)
{
    encInfo->image_capacity = get_image_size_for_bmp(encInfo->fptr_src_image);
    encInfo->size_secret_file = get_file_size(encInfo->fptr_secret);
    int required_capacity = (54 + strlen(MAGIC_STRING) * 8 + 32 + strlen(encInfo->extn_secret_file) * 8 + 32 + encInfo->size_secret_file * 8);
    
    if (encInfo->image_capacity >= required_capacity)
        return e_success;
    else
        return e_failure;
}


Status encode_secret_file_extn(char *file_extn, EncodeInfo *encInfo)
{
    encode_data_to_image(file_extn, strlen(file_extn), encInfo->fptr_src_image, encInfo->fptr_stego_image, encInfo);
    return e_success;
}

Status encode_secret_file_size(int size, EncodeInfo *encInfo)
{
    char str[32];
    fread(str, 32, 1, encInfo->fptr_src_image);
    encode_size_to_lsb(size, str);
    fwrite(str, 32, 1, encInfo->fptr_stego_image);
    return e_success;
}


Status encode_secret_file_data(EncodeInfo *encInfo)
{
    fseek(encInfo->fptr_secret, 0, SEEK_SET);
    char str[encInfo->size_secret_file];
    fread(str, encInfo->size_secret_file, 1, encInfo->fptr_secret);
    encode_data_to_image(str, strlen(str), encInfo->fptr_src_image, encInfo->fptr_stego_image, encInfo);
    return e_success;
}
/* Get the size of a file */
long get_file_size(FILE *fptr)
{
    fseek(fptr, 0, SEEK_END);
    long size = ftell(fptr);
    fseek(fptr, 0, SEEK_SET); // Reset file pointer to the beginning
    return size;
}

/* Copy BMP header (first 54 bytes) */
Status copy_bmp_header(FILE *fptr_src_image, FILE *fptr_dest_image)
{
    char header[54];
    fseek(fptr_src_image, 0, SEEK_SET);
    fread(header, 54, 1, fptr_src_image);
    fwrite(header, 54, 1, fptr_dest_image);
    return e_success;
}

/* Encode a byte of data into LSB of image bytes */
Status encode_byte_to_lsb(char data, char *image_buffer)
{
    uint mask = 0x80;  // Mask to extract bits of data
    for (int i = 0; i < 8; i++)
    {
        // Clear LSB of image byte and set it with data bit
        image_buffer[i] = (image_buffer[i] & 0xFE) | ((data & mask) >> (7 - i));
        mask >>= 1;
    }
    return e_success;
}

/* Encode data into the image */
Status encode_data_to_image(char *data, int size, FILE *fptr_src_image, FILE *fptr_stego_image, EncodeInfo *encInfo)
{
    // char image_buffer[8];
    for (int i = 0; i < size; i++)
    {
        // Read 8 bytes of image data
        fread(encInfo -> image_data, 8, 1, fptr_src_image);

        // Encode 1 byte of data into these 8 bytes
        encode_byte_to_lsb(data[i], encInfo -> image_data);

        // Write the modified 8 bytes back to the stego image
        fwrite(encInfo -> image_data, 8, 1, fptr_stego_image);
    }
    return e_success;
}

/* Encode the size of the secret file extension */
Status encode_secret_file_extn_size(int size, FILE *fptr_src_image, FILE *fptr_stego_image)
{
    char image_buffer[32];
    fread(image_buffer, 32, 1, fptr_src_image);
    encode_size_to_lsb(size, image_buffer);
    fwrite(image_buffer, 32, 1, fptr_stego_image);
    return e_success;
}

/* Encode an integer (like size) into LSBs */
Status encode_size_to_lsb(int size, char *image_buffer)
{
    unsigned int mask = 1 << 31;
    for (int i = 0; i < 32; i++)
    {
        image_buffer[i] = (image_buffer[i] & 0xFE) | ((size & mask) >> (31 - i));
        mask >>= 1;
    }
    return e_success;
}

/* Copy remaining image data after encoding */
Status copy_remaining_img_data(FILE *fptr_src, FILE *fptr_dest)
{
    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fptr_src)) > 0)
    {
        fwrite(buffer, 1, bytes, fptr_dest);
    }
    return e_success;
}

/* Main function for encoding process */
Status do_encoding(EncodeInfo *encInfo)
{
    if (open_files(encInfo) == e_success)
    {
        printf("Opened files successfully\n");

        if (check_capacity(encInfo) == e_success)
        {
            printf("Checked capacity successfully\n");

            if (copy_bmp_header(encInfo->fptr_src_image, encInfo->fptr_stego_image) == e_success)
            {
                printf("Copied BMP header successfully\n");

                if (encode_magic_string(MAGIC_STRING, encInfo) == e_success)
                {
                    printf("Encoded magic string successfully\n");

                    strcpy(encInfo->extn_secret_file, strstr(encInfo->secret_fname, "."));
                    if (encode_secret_file_extn_size(strlen(encInfo->extn_secret_file), encInfo->fptr_src_image, encInfo->fptr_stego_image) == e_success)
                    {
                        printf("Encoded secret file extension size successfully\n");

                        if (encode_secret_file_extn(encInfo->extn_secret_file, encInfo) == e_success)
                        {
                            printf("Encoded secret file extension successfully\n");

                            if (encode_secret_file_size(encInfo->size_secret_file, encInfo) == e_success)
                            {
                                printf("Encoded secret file size successfully\n");

                                if (encode_secret_file_data(encInfo) == e_success)
                                {
                                    printf("Encoded secret file data successfully\n");

                                    if (copy_remaining_img_data(encInfo->fptr_src_image, encInfo->fptr_stego_image) == e_success)
                                    {
                                        printf("Copied remaining image data successfully\n");
                                        return e_success;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return e_failure;
}

