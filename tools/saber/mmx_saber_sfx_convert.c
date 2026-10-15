#include "mmx_saber_sfx.h"
#include "mmx_saber_sfx_codec.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* stb_vorbis is deliberately a source on this target only.  The game target
 * sees the sidecar parser, never this declaration or the decoder object. */
extern int stb_vorbis_decode_memory(const unsigned char *mem, int len,
                                    int *channels, int *sample_rate,
                                    short **output);

_Static_assert(sizeof(short) == sizeof(int16_t),
               "stb_vorbis short output must be 16-bit");

enum {
  MAX_INPUT_BYTES = 64 * 1024 * 1024,
  /* Operator knob: effective rate is source rate * UPPERCUT_PITCH / 100. */
  UPPERCUT_PITCH = 126,
  UPPERCUT_PITCH_DENOMINATOR = 100
};

typedef struct ClipSpec {
  const char *file_name;
  uint32_t pitch_numerator;
  uint32_t pitch_denominator;
} ClipSpec;

static const ClipSpec kClipSpecs[MMX_SABER_SFX_CLIP_COUNT] = {
  {"saber_1.ogg", 1, 1},
  {"saber_2.ogg", 1, 1},
  {"saber_3.ogg", 1, 1},
  {"crescent_slash.wav", 1, 1},
  {"saber_1.ogg", UPPERCUT_PITCH,
      UPPERCUT_PITCH_DENOMINATOR}
};

typedef struct DecodedClip {
  int16_t *samples;
  uint32_t channels;
  uint32_t sample_rate;
  uint32_t frame_count;
  uint32_t byte_length;
  MmxSaberSfxBrrStats brr_stats;
  uint32_t source_rate;
  uint32_t effective_rate;
  size_t source_frames;
} DecodedClip;

static void put16(uint8_t *p, uint16_t value) {
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
}
static void put32(uint8_t *p, uint32_t value) {
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
  p[2] = (uint8_t)(value >> 16);
  p[3] = (uint8_t)(value >> 24);
}

static void free_clips(DecodedClip clips[MMX_SABER_SFX_CLIP_COUNT]) {
  for (unsigned i = 0; i < MMX_SABER_SFX_CLIP_COUNT; ++i) {
    free(clips[i].samples);
    clips[i].samples = NULL;
  }
}

static int read_file(const char *path, uint8_t **out, size_t *out_size) {
  FILE *file = NULL;
  long end;
  size_t size;
  uint8_t *data;
  if (!path || !out || !out_size) return 0;
  *out = NULL;
  *out_size = 0;
  file = fopen(path, "rb");
  if (!file || fseek(file, 0, SEEK_END) != 0 ||
      (end = ftell(file)) < 0 || end > MAX_INPUT_BYTES ||
      fseek(file, 0, SEEK_SET) != 0) {
    if (file) fclose(file);
    return 0;
  }
  size = (size_t)end;
  data = (uint8_t *)malloc(size ? size : 1);
  if (!data || (size && fread(data, 1, size, file) != size) ||
      ferror(file)) {
    free(data);
    fclose(file);
    return 0;
  }
  if (fclose(file) != 0) {
    free(data);
    return 0;
  }
  *out = data;
  *out_size = size;
  return 1;
}

static int path_join(char *out, size_t cap, const char *dir,
                     const char *leaf) {
  int written;
  size_t dir_len;
  if (!out || !cap || !dir || !leaf) return 0;
  dir_len = strlen(dir);
  if (dir_len && (dir[dir_len - 1] == '/' || dir[dir_len - 1] == '\\'))
    written = snprintf(out, cap, "%s%s", dir, leaf);
  else
    written = snprintf(out, cap, "%s/%s", dir, leaf);
  return written >= 0 && (size_t)written < cap;
}

static uint16_t get16(const uint8_t *p) {
  return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
      ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void set_reason(char *reason, size_t reason_size, const char *message) {
  if (reason && reason_size)
    snprintf(reason, reason_size, "%s", message ? message : "conversion failed");
}

static int extension_is(const char *path, const char *extension) {
  const char *dot = path ? strrchr(path, '.') : NULL;
  if (!dot || !extension) return 0;
  if (!strcmp(dot, extension)) return 1;
  if (!strcmp(extension, ".ogg")) return !strcmp(dot, ".OGG");
  if (!strcmp(extension, ".wav")) return !strcmp(dot, ".WAV");
  return 0;
}

static int decode_wav(const uint8_t *data, size_t size, short **out,
                      unsigned *channels, uint32_t *sample_rate,
                      size_t *frames, char *reason, size_t reason_size) {
  const uint8_t *pcm = NULL;
  size_t pcm_size = 0;
  size_t riff_end, cursor;
  uint32_t riff_size;
  uint16_t wav_channels = 0, block_align = 0, bits = 0, format = 0;
  uint32_t wav_rate = 0, byte_rate = 0;
  bool have_fmt = false, have_data = false;
  short *decoded = NULL;
  size_t frame_count, sample_count;

  if (!data || !out || !channels || !sample_rate || !frames) {
    set_reason(reason, reason_size, "invalid WAV: missing decoder output");
    return 0;
  }
  *out = NULL;
  *channels = 0;
  *sample_rate = 0;
  *frames = 0;
  if (size < 12 || memcmp(data, "RIFF", 4) != 0 ||
      memcmp(data + 8, "WAVE", 4) != 0) {
    set_reason(reason, reason_size, "invalid WAV: missing RIFF/WAVE header");
    return 0;
  }
  riff_size = get32(data + 4);
  if (riff_size < 4 || (uint64_t)riff_size > (uint64_t)size - 8) {
    set_reason(reason, reason_size, "invalid WAV: RIFF size is out of bounds");
    return 0;
  }
  riff_end = (size_t)riff_size + 8;
  cursor = 12;
  while (cursor < riff_end) {
    size_t chunk_data, padded_size;
    uint32_t chunk_size;
    if (riff_end - cursor < 8) {
      set_reason(reason, reason_size,
                 "invalid WAV: truncated chunk header");
      return 0;
    }
    chunk_size = get32(data + cursor + 4);
    chunk_data = cursor + 8;
    if ((uint64_t)chunk_size > (uint64_t)(riff_end - chunk_data)) {
      set_reason(reason, reason_size,
                 "invalid WAV: chunk exceeds RIFF bounds");
      return 0;
    }
    padded_size = (size_t)chunk_size;
    if (padded_size & 1u) {
      if (padded_size == SIZE_MAX) {
        set_reason(reason, reason_size,
                   "invalid WAV: chunk padding overflows");
        return 0;
      }
      ++padded_size;
    }
    if (padded_size > riff_end - chunk_data) {
      set_reason(reason, reason_size,
                 "invalid WAV: chunk padding exceeds RIFF bounds");
      return 0;
    }
    if (!memcmp(data + cursor, "fmt ", 4)) {
      uint64_t expected_byte_rate;
      if (have_fmt) {
        set_reason(reason, reason_size, "invalid WAV: duplicate fmt chunk");
        return 0;
      }
      if (chunk_size < 16) {
        set_reason(reason, reason_size, "invalid WAV: truncated fmt chunk");
        return 0;
      }
      format = get16(data + chunk_data);
      wav_channels = get16(data + chunk_data + 2);
      wav_rate = get32(data + chunk_data + 4);
      byte_rate = get32(data + chunk_data + 8);
      block_align = get16(data + chunk_data + 12);
      bits = get16(data + chunk_data + 14);
      if (format != 1) {
        set_reason(reason, reason_size,
                   "unsupported WAV: fmt format is not PCM");
        return 0;
      }
      if (wav_channels != 1 && wav_channels != 2) {
        set_reason(reason, reason_size,
                   "unsupported WAV: channels must be mono or stereo");
        return 0;
      }
      if (bits != 16) {
        set_reason(reason, reason_size,
                   "unsupported WAV: sample width is not 16-bit");
        return 0;
      }
      if (!wav_rate || block_align != (uint16_t)(wav_channels * 2)) {
        set_reason(reason, reason_size,
                   "invalid WAV: sample rate or block alignment");
        return 0;
      }
      expected_byte_rate = (uint64_t)wav_rate * block_align;
      if (expected_byte_rate > UINT32_MAX ||
          byte_rate != (uint32_t)expected_byte_rate) {
        set_reason(reason, reason_size,
                   "invalid WAV: byte rate does not match PCM format");
        return 0;
      }
      have_fmt = true;
    } else if (!memcmp(data + cursor, "data", 4)) {
      if (have_data) {
        set_reason(reason, reason_size, "invalid WAV: duplicate data chunk");
        return 0;
      }
      pcm = data + chunk_data;
      pcm_size = chunk_size;
      have_data = true;
    }
    cursor = chunk_data + padded_size;
  }
  if (!have_fmt) {
    set_reason(reason, reason_size, "invalid WAV: fmt chunk is missing");
    return 0;
  }
  if (!have_data) {
    set_reason(reason, reason_size, "invalid WAV: data chunk is missing");
    return 0;
  }
  if (!pcm_size) {
    set_reason(reason, reason_size, "invalid WAV: data chunk is empty");
    return 0;
  }
  if (pcm_size % block_align != 0) {
    set_reason(reason, reason_size,
               "invalid WAV: data is not frame-aligned");
    return 0;
  }
  frame_count = pcm_size / block_align;
  if (frame_count > SIZE_MAX / wav_channels) {
    set_reason(reason, reason_size, "invalid WAV: frame count overflows");
    return 0;
  }
  sample_count = frame_count * wav_channels;
  if (sample_count > SIZE_MAX / sizeof(*decoded)) {
    set_reason(reason, reason_size, "invalid WAV: sample count overflows");
    return 0;
  }
  decoded = (short *)malloc(sample_count * sizeof(*decoded));
  if (!decoded) {
    set_reason(reason, reason_size, "invalid WAV: sample allocation failed");
    return 0;
  }
  for (size_t i = 0; i < sample_count; ++i)
    decoded[i] = (short)(int16_t)get16(pcm + i * sizeof(int16_t));
  *out = decoded;
  *channels = wav_channels;
  *sample_rate = wav_rate;
  *frames = frame_count;
  return 1;
}

static int16_t downmix_sample(const short *decoded, size_t frame,
                              unsigned channels) {
  if (channels == 1) return decoded[frame];
  /* Both input decoders provide mono/stereo output. Symmetric integer
   * averaging keeps the downmix independent of the host FPU. */
  return (int16_t)(((int32_t)decoded[frame * 2] +
                    (int32_t)decoded[frame * 2 + 1]) / 2);
}

static int32_t to_dsp15(int16_t sample) {
  int32_t value = sample;
  if (value >= 0) return value / 2;
  return -(((-value) + 1) / 2);
}

static int effective_sample_rate(uint32_t source_rate, const ClipSpec *spec,
                                 uint32_t *out_rate, char *reason,
                                 size_t reason_size) {
  uint64_t scaled_rate;
  if (!source_rate || !spec || !spec->pitch_numerator ||
      !spec->pitch_denominator) {
    set_reason(reason, reason_size,
               "invalid audio pitch factor or source sample rate");
    return 0;
  }
  scaled_rate = (uint64_t)source_rate * spec->pitch_numerator /
      spec->pitch_denominator;
  if (!scaled_rate || scaled_rate > UINT32_MAX) {
    set_reason(reason, reason_size,
               "audio pitch factor produces an invalid sample rate");
    return 0;
  }
  *out_rate = (uint32_t)scaled_rate;
  return 1;
}

static int decode_clip(const char *path, const ClipSpec *spec,
                       DecodedClip *clip, char *reason, size_t reason_size) {
  uint8_t *encoded = NULL;
  size_t encoded_size = 0;
  unsigned channels = 0;
  int ogg_channels = 0, ogg_sample_rate = 0, ogg_frames = 0;
  uint32_t source_rate = 0, effective_rate = 0;
  size_t source_frames = 0;
  short *decoded = NULL;
  int16_t *mono = NULL, *resampled = NULL, *dsp_input = NULL;
  int16_t *brr_decoded = NULL, *rendered = NULL;
  uint8_t *brr = NULL;
  size_t mono_bytes, resampled_frames, brr_bytes, brr_frames, rendered_frames;
  MmxSaberSfxBrrStats stats;

  if (reason && reason_size) reason[0] = '\0';
  if (!clip || !spec) {
    set_reason(reason, reason_size, "missing clip specification");
    return 0;
  }
  if (!read_file(path, &encoded, &encoded_size)) {
    set_reason(reason, reason_size, "cannot read audio input");
    free(encoded);
    return 0;
  }
  if (extension_is(path, ".wav")) {
    if (!decode_wav(encoded, encoded_size, &decoded, &channels, &source_rate,
                    &source_frames, reason, reason_size)) {
      free(encoded);
      return 0;
    }
  } else if (extension_is(path, ".ogg")) {
    if (encoded_size > INT_MAX) {
      set_reason(reason, reason_size, "OGG input exceeds decoder size limit");
      free(encoded);
      return 0;
    }
    ogg_frames = stb_vorbis_decode_memory(
        encoded, (int)encoded_size, &ogg_channels, &ogg_sample_rate, &decoded);
    if (ogg_frames <= 0 || (ogg_channels != 1 && ogg_channels != 2) ||
        ogg_sample_rate < 8000 || ogg_sample_rate > 192000 || !decoded) {
      set_reason(reason, reason_size, "invalid OGG/Vorbis audio");
      free(decoded);
      free(encoded);
      return 0;
    }
    channels = (unsigned)ogg_channels;
    source_rate = (uint32_t)ogg_sample_rate;
    source_frames = (size_t)ogg_frames;
  } else {
    set_reason(reason, reason_size,
               "unsupported audio extension; expected .ogg or .wav");
    free(encoded);
    return 0;
  }
  free(encoded);
  encoded = NULL;
  if (source_frames > SIZE_MAX / sizeof(*mono) ||
      !effective_sample_rate(source_rate, spec, &effective_rate, reason,
                             reason_size)) {
    free(decoded);
    return 0;
  }
  mono_bytes = source_frames * sizeof(*mono);
  mono = (int16_t *)malloc(mono_bytes);
  if (!mono) goto fail;
  for (size_t i = 0; i < source_frames; ++i)
    mono[i] = downmix_sample(decoded, i, channels);
  free(decoded);
  decoded = NULL;

  if (!MmxSaberSfxResampleMono16k(mono, source_frames, effective_rate,
                                  &resampled,
                                  &resampled_frames)) goto fail;
  free(mono);
  mono = NULL;
  if (resampled_frames > SIZE_MAX / sizeof(*dsp_input)) goto fail;
  dsp_input = (int16_t *)malloc(resampled_frames * sizeof(*dsp_input));
  if (!dsp_input) goto fail;
  for (size_t i = 0; i < resampled_frames; ++i)
    dsp_input[i] = (int16_t)to_dsp15(resampled[i]);

  if (!MmxSaberSfxBrrEncode(dsp_input, resampled_frames, &brr, &brr_bytes,
                            &stats)) goto fail;
  free(dsp_input);
  dsp_input = NULL;
  if (brr_bytes % 9 != 0) goto fail;
  brr_frames = (brr_bytes / 9) * 16;
  if (brr_frames > SIZE_MAX / sizeof(*brr_decoded)) goto fail;
  brr_decoded = (int16_t *)malloc(brr_frames * sizeof(*brr_decoded));
  if (!brr_decoded || !MmxSaberSfxBrrDecode(brr, brr_bytes, brr_decoded,
                                             brr_frames)) goto fail;
  if (!MmxSaberSfxGaussianRender(brr_decoded, resampled_frames, &rendered,
                                 &rendered_frames)) goto fail;
  if (rendered_frames > UINT32_MAX ||
      rendered_frames > UINT32_MAX / sizeof(int16_t)) goto fail;

  free(resampled);
  free(brr_decoded);
  free(brr);
  memset(clip, 0, sizeof(*clip));
  clip->samples = rendered;
  clip->channels = MMX_SABER_SFX_CHANNELS;
  clip->sample_rate = MMX_SABER_SFX_NATIVE_SAMPLE_RATE;
  clip->frame_count = (uint32_t)rendered_frames;
  clip->byte_length = (uint32_t)(rendered_frames * sizeof(int16_t));
  clip->brr_stats = stats;
  clip->source_rate = source_rate;
  clip->effective_rate = effective_rate;
  clip->source_frames = source_frames;
  return 1;

fail:
  if (!reason || !reason[0])
    set_reason(reason, reason_size, "audio conversion failed");
  free(encoded);
  free(decoded);
  free(mono);
  free(resampled);
  free(dsp_input);
  free(brr_decoded);
  free(rendered);
  free(brr);
  return 0;
}

static int build_sidecar(const DecodedClip clips[MMX_SABER_SFX_CLIP_COUNT],
                         uint8_t **out, size_t *out_size) {
  uint64_t total = 0;
  size_t size, cursor;
  uint8_t *data;
  if (!clips || !out || !out_size) return 0;
  for (unsigned i = 0; i < MMX_SABER_SFX_CLIP_COUNT; ++i) {
    if (clips[i].channels != MMX_SABER_SFX_CHANNELS ||
        clips[i].sample_rate != MMX_SABER_SFX_NATIVE_SAMPLE_RATE ||
        !clips[i].frame_count) return 0;
    total += clips[i].byte_length;
    if (total > MMX_SABER_SFX_MAX_PCM_BYTES) return 0;
  }
  size = MMX_SABER_SFX_HEADER_BYTES +
      (size_t)MMX_SABER_SFX_CLIP_COUNT * MMX_SABER_SFX_RECORD_BYTES +
      (size_t)total;
  if (size > UINT32_MAX) return 0;
  data = (uint8_t *)calloc(size, 1);
  if (!data) return 0;
  memcpy(data, "MMXSFX2", 7);
  put16(data + 8, MMX_SABER_SFX_VERSION);
  put16(data + 10, MMX_SABER_SFX_HEADER_BYTES);
  put16(data + 12, MMX_SABER_SFX_CLIP_COUNT);
  put16(data + 14, MMX_SABER_SFX_RECORD_BYTES);
  put32(data + 16, (uint32_t)size);
  put32(data + 20, (uint32_t)total);
  cursor = MMX_SABER_SFX_HEADER_BYTES +
      (size_t)MMX_SABER_SFX_CLIP_COUNT * MMX_SABER_SFX_RECORD_BYTES;
  for (unsigned i = 0; i < MMX_SABER_SFX_CLIP_COUNT; ++i) {
    uint8_t *record = data + MMX_SABER_SFX_HEADER_BYTES +
        (size_t)i * MMX_SABER_SFX_RECORD_BYTES;
    put32(record + 0, clips[i].channels);
    put32(record + 4, clips[i].sample_rate);
    put32(record + 8, clips[i].frame_count);
    put32(record + 12, clips[i].byte_length);
    for (uint32_t j = 0; j < clips[i].byte_length / sizeof(int16_t); ++j)
      put16(data + cursor + (size_t)j * sizeof(int16_t),
            (uint16_t)clips[i].samples[j]);
    cursor += clips[i].byte_length;
  }
  *out = data;
  *out_size = size;
  return true;
}

static int write_file(const char *path, const uint8_t *data, size_t size) {
  FILE *file;
  int ok;
  if (!path || !data) return 0;
  file = fopen(path, "wb");
  if (!file) return 0;
  ok = fwrite(data, 1, size, file) == size;
  if (fclose(file) != 0) ok = 0;
  return ok;
}

static int preview_leaf(char *out, size_t cap, unsigned clip_id,
                        const char *file_name) {
  const char *base = file_name;
  const char *slash;
  const char *backslash;
  const char *dot;
  size_t stem_length;
  int written;
  if (!out || !cap || !file_name) return 0;
  slash = strrchr(base, '/');
  backslash = strrchr(base, '\\');
  if (slash && (!backslash || slash > backslash)) base = slash + 1;
  else if (backslash) base = backslash + 1;
  dot = strrchr(base, '.');
  stem_length = dot && dot > base ? (size_t)(dot - base) : strlen(base);
  if (stem_length > INT_MAX) return 0;
  written = snprintf(out, cap, "clip%u_%.*s.wav", clip_id,
                     (int)stem_length, base);
  return written >= 0 && (size_t)written < cap;
}

static int write_preview_wav(const char *path, const DecodedClip *clip) {
  uint8_t *data;
  size_t size;
  if (!path || !clip || !clip->samples || !clip->frame_count ||
      clip->channels != 1 || clip->frame_count > UINT32_MAX / 2 ||
      clip->byte_length != clip->frame_count * sizeof(int16_t) ||
      clip->sample_rate > UINT32_MAX / 2) return 0;
  if ((uint64_t)clip->byte_length + 44u > SIZE_MAX ||
      (uint64_t)clip->byte_length + 36u > UINT32_MAX) return 0;
  size = 44u + clip->byte_length;
  data = (uint8_t *)calloc(size, 1);
  if (!data) return 0;
  memcpy(data + 0, "RIFF", 4);
  put32(data + 4, (uint32_t)(size - 8));
  memcpy(data + 8, "WAVEfmt ", 8);
  put32(data + 16, 16);
  put16(data + 20, 1);
  put16(data + 22, 1);
  put32(data + 24, clip->sample_rate);
  put32(data + 28, clip->sample_rate * 2);
  put16(data + 32, 2);
  put16(data + 34, 16);
  memcpy(data + 36, "data", 4);
  put32(data + 40, clip->byte_length);
  for (uint32_t i = 0; i < clip->frame_count; ++i)
    put16(data + 44u + (size_t)i * sizeof(int16_t),
          (uint16_t)clip->samples[i]);
  if (!write_file(path, data, size)) {
    free(data);
    return 0;
  }
  free(data);
  return 1;
}

static int write_previews(const char *preview_dir,
                          const DecodedClip clips[MMX_SABER_SFX_CLIP_COUNT]) {
  char leaf[256];
  char path[4096];
  if (!preview_dir || !clips) return 0;
  for (unsigned i = 0; i < MMX_SABER_SFX_CLIP_COUNT; ++i) {
    if (!preview_leaf(leaf, sizeof(leaf), i, kClipSpecs[i].file_name) ||
        !path_join(path, sizeof(path), preview_dir, leaf) ||
        !write_preview_wav(path, &clips[i]))
      return 0;
  }
  return 1;
}

static void usage(const char *program) {
  fprintf(stderr,
          "usage: %s --source-dir DIR --out FILE [--preview-dir DIR]\n",
          program);
}

int main(int argc, char **argv) {
  const char *source_dir = NULL, *output_path = NULL, *preview_dir = NULL;
  DecodedClip clips[MMX_SABER_SFX_CLIP_COUNT] = {0};
  uint8_t *sidecar = NULL;
  size_t sidecar_size = 0;
  char path[4096];
  char reason[256];
  int ok = 0;

  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--source-dir") && i + 1 < argc)
      source_dir = argv[++i];
    else if (!strcmp(argv[i], "--out") && i + 1 < argc)
      output_path = argv[++i];
    else if (!strcmp(argv[i], "--preview-dir") && i + 1 < argc)
      preview_dir = argv[++i];
    else {
      usage(argv[0]);
      return 2;
    }
  }
  if (!source_dir || !output_path) {
    usage(argv[0]);
    return 2;
  }
  for (unsigned i = 0; i < MMX_SABER_SFX_CLIP_COUNT; ++i) {
    reason[0] = '\0';
    if (!path_join(path, sizeof(path), source_dir, kClipSpecs[i].file_name)) {
      set_reason(reason, sizeof(reason), "input path is too long");
    } else if (!decode_clip(path, &kClipSpecs[i], &clips[i], reason,
                            sizeof(reason))) {
      /* decode_clip supplies format-specific detail, including strict WAV
       * rejection reasons. */
      if (!reason[0]) set_reason(reason, sizeof(reason), "audio conversion failed");
    }
    if (reason[0]) {
      fprintf(stderr,
              "mmx_saber_sfx_convert: failed to decode clip %u (%s): %s\n",
              i, kClipSpecs[i].file_name, reason);
      goto done;
    }
    fprintf(stderr,
        "mmx_saber_sfx_convert: clip %u input=%zu@%uHz, resample=%uHz, "
        "output=%u@%uHz, "
        "BRR blocks=%zu filters=%u/%u/%u/%u\n", i + 1,
        clips[i].source_frames, clips[i].source_rate, clips[i].effective_rate,
        clips[i].frame_count, clips[i].sample_rate,
        clips[i].brr_stats.block_count,
        clips[i].brr_stats.filter_counts[0], clips[i].brr_stats.filter_counts[1],
        clips[i].brr_stats.filter_counts[2], clips[i].brr_stats.filter_counts[3]);
  }
  if (preview_dir && !write_previews(preview_dir, clips)) {
    fprintf(stderr, "mmx_saber_sfx_convert: failed to write previews\n");
    goto done;
  }
  if (!build_sidecar(clips, &sidecar, &sidecar_size) ||
      !write_file(output_path, sidecar, sidecar_size)) {
    fprintf(stderr, "mmx_saber_sfx_convert: failed to write output\n");
    goto done;
  }
  ok = 1;
done:
  free(sidecar);
  free_clips(clips);
  return ok ? 0 : 1;
}
