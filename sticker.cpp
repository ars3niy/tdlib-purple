#include "sticker.h"
#include "buildopt.h"
#include "config.h"
#include "gif.h"
#include "format.h"

#ifndef NoWebp
#include <png.h>
#include <webp/decode.h>
#endif

#ifndef NoLottie
#include <zlib.h>
#include <rlottie.h>
#endif

constexpr int MAX_W = 256;
constexpr int MAX_H = 256;
constexpr unsigned ANIMATED_WIDTH  = 200;
constexpr unsigned ANIMATED_HEIGHT = 200;

#ifndef NoWebp

static void p2tgl_png_mem_write (png_structp png_ptr, png_bytep data, png_size_t length)
{
    GByteArray *png_mem = (GByteArray *) png_get_io_ptr(png_ptr);
    g_byte_array_append (png_mem, data, length);
}

static int p2tgl_imgstore_add_with_id_png (const unsigned char *raw_bitmap, unsigned width, unsigned height)
{
    GByteArray *png_mem = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytepp rows = NULL;

    // init png write struct
    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        purple_debug_misc(config::pluginId, "error encoding png (create_write_struct failed)\n");
        return 0;
    }

    // init png info struct
    info_ptr = png_create_info_struct (png_ptr);
    if (info_ptr == NULL) {
        png_destroy_write_struct(&png_ptr, NULL);
        purple_debug_misc(config::pluginId, "error encoding png (create_info_struct failed)\n");
        return 0;
    }

    // Set up error handling.
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        purple_debug_misc(config::pluginId, "error while writing png\n");
        return 0;
    }

    // set img attributes
    png_set_IHDR (png_ptr, info_ptr, width, height,
                    8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                    PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    // alloc row pointers
    rows = g_new0 (png_bytep, height);
    if (rows == NULL) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        purple_debug_misc(config::pluginId, "error converting to png: malloc failed\n");
        return 0;
    }

    unsigned i;
    for (i = 0; i < height; i++)
        rows[i] = (png_bytep)(raw_bitmap + i * width * 4);

    // create array and set own png write function
    png_mem = g_byte_array_new();
    png_set_write_fn (png_ptr, png_mem, p2tgl_png_mem_write, NULL);

    // write png
    png_set_rows (png_ptr, info_ptr, rows);
    png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    // cleanup
    g_free(rows);
    png_destroy_write_struct (&png_ptr, &info_ptr);
    unsigned png_size = png_mem->len;
    gpointer png_data = g_byte_array_free (png_mem, FALSE);

    return purple_imgstore_add_with_id (png_data, png_size, NULL);
}

static int p2tgl_imgstore_add_with_id_webp (const char *filename)
{
    const uint8_t *data = NULL;
    size_t len;
    GError *err = NULL;
    g_file_get_contents (filename, (gchar **) &data, &len, &err);
    if (err) {
        purple_debug_misc(config::pluginId, "cannot open file %s: %s\n", filename, err->message);
        g_error_free(err);
        return 0;
    }

    // downscale oversized sticker images displayed in chat, otherwise it would harm readabillity
    WebPDecoderConfig config;
    WebPInitDecoderConfig (&config);
    if (WebPGetFeatures(data, len, &config.input) != VP8_STATUS_OK) {
        purple_debug_misc(config::pluginId, "error reading webp bitstream: %s\n", filename);
        g_free ((gchar *)data);
        return 0;
    }

    config.options.use_scaling = 0;
    config.options.scaled_width = config.input.width;
    config.options.scaled_height = config.input.height;
    if (config.options.scaled_width > MAX_W || config.options.scaled_height > MAX_H) {
        const float max_scale_width = MAX_W * 1.0f / config.options.scaled_width;
        const float max_scale_height = MAX_H * 1.0f / config.options.scaled_height;
        if (max_scale_width < max_scale_height) {
        // => the width is most limiting
        config.options.scaled_width = MAX_W;
        // Can't use ' *= ', because we need to do the multiplication in float
        // (or double), and only THEN cast back to int.
        config.options.scaled_height = (int) (config.options.scaled_height * max_scale_width);
        } else {
        // => the height is most limiting
        config.options.scaled_height = MAX_H;
        // Can't use ' *= ', because we need to do the multiplication in float
        // (or double), and only THEN cast back to int.
        config.options.scaled_width = (int) (config.options.scaled_width * max_scale_height);
        }
        config.options.use_scaling = 1;
    }
    config.output.colorspace = MODE_RGBA;
    if (WebPDecode(data, len, &config) != VP8_STATUS_OK) {
        purple_debug_misc(config::pluginId, "error decoding webp: %s\n", filename);
        g_free ((gchar *)data);
        return 0;
    }
    g_free ((gchar *)data);
    const uint8_t *decoded = config.output.u.RGBA.rgba;

    // convert and add
    int imgStoreId = p2tgl_imgstore_add_with_id_png(decoded, config.options.scaled_width, config.options.scaled_height);
    WebPFreeDecBuffer (&config.output);
    return imgStoreId;
}

#else

int p2tgl_imgstore_add_with_id_webp (const char *filename)
{
    return 0;
}

#endif

void showWebpSticker(const td::td_api::chat &chat, const TgMessageInfo &message,
                     const std::string &filePath, const std::string &fileDescription,
                     TdAccountData &account)
{
    int id = p2tgl_imgstore_add_with_id_webp(filePath.c_str());
    if (id != 0) {
        std::string text = "\n<img id=\"" + std::to_string(id) + "\">";
        showMessageText(account, chat, message, text.c_str(), NULL);
    } else
        showGenericFileInline(chat, message, filePath, fileDescription, account);
}

#ifndef NoLottie

static bool gunzip(gchar *compressedData, gsize compressedSize, std::string &output,
                   std::string &errorMessage)
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    int unzipResult = inflateInit2(&strm, MAX_WBITS + 16);
    if (unzipResult != Z_OK) {
        // Unlikely error message not worth translating
        errorMessage = "Failed to initialize unzip stream";
        return false;
    }

    if (compressedSize) {
        char unzipBuffer[16384];
        strm.avail_in = compressedSize;
        strm.next_in  = reinterpret_cast<uint8_t *>(compressedData);
        do {
            strm.avail_out = sizeof(unzipBuffer);
            strm.next_out = reinterpret_cast<uint8_t *>(unzipBuffer);
            unzipResult = inflate(&strm, Z_NO_FLUSH);
            if ((unzipResult != Z_OK) && (unzipResult != Z_STREAM_END))
                break;

            if (strm.avail_out > sizeof(unzipBuffer)) {
                unzipResult = Z_STREAM_ERROR;
                break;
            }
            unsigned have = sizeof(unzipBuffer) - strm.avail_out;
            output.append(unzipBuffer, have);
        } while (strm.avail_out == 0);
    }
    (void)inflateEnd(&strm);

    if ((unzipResult != Z_OK) && (unzipResult != Z_STREAM_END)) {
        // Unlikely error message not worth translating
        errorMessage = "Decompression error";
        return false;
    }
    return true;
}

class GifBuilder {
public:
    explicit GifBuilder(int fd, const uint32_t width,
                        const uint32_t height, const int bgColor=0xffffffff, const uint32_t delay = 2)
    {
        GifBegin(&handle, fd, width, height, delay);
        bgColorR = (uint8_t) ((bgColor & 0xff0000) >> 16);
        bgColorG = (uint8_t) ((bgColor & 0x00ff00) >> 8);
        bgColorB = (uint8_t) ((bgColor & 0x0000ff));
    }
    ~GifBuilder()
    {
        GifEnd(&handle);
    }
    void addFrame(rlottie::Surface &s, uint32_t delay = 2)
    {
        argbTorgba(s);
        GifWriteFrame(&handle,
                      reinterpret_cast<uint8_t *>(s.buffer()),
                      s.width(),
                      s.height(),
                      delay);
    }
    void argbTorgba(rlottie::Surface &s)
    {
        uint8_t *buffer = reinterpret_cast<uint8_t *>(s.buffer());
        uint32_t totalBytes = s.height() * s.bytesPerLine();

        for (uint32_t i = 0; i < totalBytes; i += 4) {
           unsigned char a = buffer[i+3];
           // compute only if alpha is non zero
           if (a) {
               unsigned char r = buffer[i+2];
               unsigned char b = buffer[i];
               buffer[i] = r;
               buffer[i+2] = b;
           } else {
               buffer[i+2] = bgColorB;
               buffer[i+1] = bgColorG;
               buffer[i] = bgColorR;
           }
        }
    }

private:
    GifWriter      handle;
    uint8_t bgColorR, bgColorG, bgColorB;
};

void StickerConversionThread::run()
{
    gchar  *compressedData = NULL;
    gsize   compressedSize = 0;
    GError *error = NULL;

    g_file_get_contents(inputFileName.c_str(), &compressedData, &compressedSize, &error);
    if (error) {
        m_errorMessage = error->message;
        g_error_free(error);
        return;
    }

    std::string lottieData;
    bool gunzipSuccess = gunzip(compressedData, compressedSize, lottieData, m_errorMessage);
    g_free(compressedData);
    if (!gunzipSuccess)
        return;

    std::unique_ptr<rlottie::Animation> player = rlottie::Animation::loadFromData(lottieData, "");
    if (!player) {
        // Unlikely error message not worth translating
        m_errorMessage = "Could not render animation";
        return;
    }

    char *tempFileName = NULL;
    int fd = g_file_open_tmp("tdlib_sticker_XXXXXX", &tempFileName, NULL);
    if (fd < 0) {
        // Unlikely error message not worth translating
        m_errorMessage = "Could not create temporary file";
        return;
    }
    m_outputFileName = tempFileName;
    g_free(tempFileName);

    unsigned w = ANIMATED_WIDTH;
    unsigned h = ANIMATED_HEIGHT;
    auto buffer = std::unique_ptr<uint32_t[]>(new uint32_t[w * h]);
    size_t frameCount = player->totalFrame();

    GifBuilder builder(fd, w, h, 0);
    for (size_t i = 0; i < frameCount ; i++) {
        rlottie::Surface surface(buffer.get(), w, h, w * 4);
        player->renderSync(i, surface);
        builder.addFrame(surface);
    }
}

#else

void StickerConversionThread::run()
{
    m_errorMessage = "Not supported";
}

#endif
