/*
    This is free and unencumbered software released into the public domain.
    
    Anyone is free to copy, modify, publish, use, compile, sell, or
    distribute this software, either in source code form or as a compiled
    binary, for any purpose, commercial or non-commercial, and by any
    means.
    
    In jurisdictions that recognize copyright laws, the author or authors
    of this software dedicate any and all copyright interest in the
    software to the public domain. We make this dedication for the benefit
    of the public at large and to the detriment of our heirs and
    successors. We intend this dedication to be an overt act of
    relinquishment in perpetuity of all present and future rights to this
    software under copyright law.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
    OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
    
    For more information, please refer to <https://unlicense.org>
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/osmesa.h>
#include <png.h>

/*
    osmesa-to-png.c
        Renders a scene into an offscreen buffer using OSMesa's OpenGL
        implementation, encodes and writes it to a PNG using libpng.
        
        This is intended to be template code for quick OpenGL rendering into
        an image file aswell as a simple implementation of a save_png()
        function, simplifying boilerplate code for libpng.
*/

/* Uses OSMesa's OpenGL implementation to render a scene. */
static void render_scene(void)
{
    /* Clear the background to be black and solid. */
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Draw a gradient triangle. */
    glBegin(GL_TRIANGLES);
        glColor3f(1.0, 0.0, 0.0);
        glVertex2f(1.0, 1.0);

        glColor3f(0.0, 1.0, 0.0);
        glVertex2f(-1.0, -1.0);
        
        glColor3f(0.0, 0.0, 1.0);
        glVertex2f(1.0, -1.0);
    glEnd();
}

/*
    Duplicates the error message string into the supplied buffer double-pointer,
    then longjumps to the error handler.
*/
static void __save_png_error(png_structp png_ptr, png_const_charp message)
{
    char **error_buffer;

    /* Handle a missing buffer double-pointer. */
    if ((error_buffer = png_get_error_ptr(png_ptr)) == NULL)
        return;
    
    /* Prevent segfault when no message was given. */
    if (message == NULL)
        message = "No message given";
    
    /* Allocate memory for the message. */
    if ((*error_buffer = malloc(strlen(message) + 1)) == NULL)
        return;

    /* Copy the message into the memory. */
    strcpy(*error_buffer, message);

    /* Jump back. */
    png_longjmp(png_ptr, 1);
}

/* Ignores the warning and longjumps to the error handler. */
static void __save_png_warn(png_structp png_ptr, png_const_charp message)
{
    /* Jump back. */
    png_longjmp(png_ptr, 1);
}

/* Encodes raw RGBA32 data into a PNG and saves it to the given path. */
static int save_png(const char *path,
    const uint32_t *rgba32_data, unsigned width, unsigned height)
{
    /* Error handling. */
    int   exit_code    = EXIT_FAILURE;
    char *error_buffer = NULL;
    
    /* I/O. */
    FILE *file;
    
    /* PNG encoding. */
    png_structp  png_ptr  = NULL;
    png_infop    info_ptr = NULL;
    const void **row_ptrs;
    int          row;

    /* Try to open the destination file. */
    if ((file = fopen(path, "wb")) == NULL)
    {
        fprintf(stderr, "error: fopen() failed: %s\n", strerror(errno));
        goto return_code;
    }

    /* Create a PNG write struct. */
    if ((png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
        &error_buffer, __save_png_error, __save_png_warn)) == NULL)
    {
        fprintf(stderr, "error: png_create_write_struct() failed.\n");
        goto close_file;
    }

    /* Create a PNG info struct. */
    if ((info_ptr = png_create_info_struct(png_ptr)) == NULL)
    {
        fprintf(stderr, "error: png_create_info_struct() failed.\n");
        goto destroy_write_ptr;
    }
    
    /* Allocate an array of row pointers. */
    if ((row_ptrs = malloc(height * sizeof(void *))) == NULL)
    {
        fprintf(stderr,
            "error: malloc() for row_ptrs failed: %s\n", strerror(errno));
        goto destroy_info_ptr;
    }

    /*
        This is the jump target in case of an error, __save_png_error() probably
        has written a pointer to an error message into error_buffer.
    */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        if (error_buffer)
        {
            fprintf(stderr, "error: (libpng) %s\n", error_buffer);
            free(error_buffer);
        }
        else
        {
            fprintf(stderr,
                "error: No error message set by __save_png_error().\n");
        }
        goto free_row_ptrs;
    }

    /* Make libpng use a standard C stream. */
    png_init_io(png_ptr, file);

    /* Set the image header's attributes. */
    png_set_IHDR(png_ptr, info_ptr,
        width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    
    /* Populate the array of row pointers. */
    for (row = 0; row < height; row++)
        row_ptrs[row] = &rgba32_data[row * width];
    
    /* Write the image's header, data and end. */
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, (png_bytepp) row_ptrs);
    png_write_end(png_ptr, NULL);

    /* The write was successful, exit successfully. */
    exit_code = EXIT_SUCCESS;

    /* Resource release chain. */
    free_row_ptrs: free(row_ptrs);
 destroy_info_ptr: png_destroy_info_struct(png_ptr, &info_ptr);
destroy_write_ptr: png_destroy_write_struct(&png_ptr, &info_ptr);
       close_file: fclose(file);
      return_code: return exit_code;
}

/* Program flow, initialization and deinitialization. */
int main(int argc, char *argv[])
{
    int width, height, exit_code = EXIT_FAILURE;
    uint32_t *rgba32_data;
    OSMesaContext context;

    /* Check argument. */
    if (argc != 4)
    {
        fprintf(stderr, "usage: osmesa-test <output-png> <width> <height>\n");
        goto return_code;
    }

    /* Parse and check the <width> and <height> arguments. */
    width = atoi(argv[2]);
    height = atoi(argv[3]);
    if (width <= 0 || height <= 0)
    {
        fprintf(stderr,
            "error: <width> and <height> must be above-zero integers.\n");
        goto return_code;
    }

    /* Allocate the raw data. */
    if ((rgba32_data = malloc(width * height * sizeof(uint32_t))) == NULL)
    {
        fprintf(stderr, 
            "error: malloc() for rgba32_data failed: %s\n", strerror(errno));
        goto return_code;
    }

    /* Create a software rendering context. */
    if ((context = OSMesaCreateContext(OSMESA_RGBA, NULL)) == NULL)
    {
        fprintf(stderr, "error: OSMesaCreateContext() failed.\n");
        goto free_rgba32_data;
    }

    /* Enable the context. */
    if (OSMesaMakeCurrent(
        context, rgba32_data, GL_UNSIGNED_BYTE, width, height) != GL_TRUE)
    {
        fprintf(stderr, "error: OSMesaMakeCurrent() failed.\n");
        goto destroy_context;
    }

    /* Render the scene, then make sure the buffer gets updated. */
    render_scene();
    glFlush();

    /* Save the render result to a PNG. */
    if (save_png(argv[1], rgba32_data, width, height) != EXIT_SUCCESS)
    {
        /* An error message will be printed by save_png(). */
        goto disable_context;
    }

    /* The task was performed, exit successfully. */
    exit_code = EXIT_SUCCESS;

    /* Resource release chain. */
 disable_context: OSMesaMakeCurrent(NULL, NULL, 0, 0, 0);
 destroy_context: OSMesaDestroyContext(context);
free_rgba32_data: free(rgba32_data);
     return_code: return exit_code;
}