#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceBuffer.h>
#include <AudioOutput.h>
#include <AudioFileSourceID3.h>

// Triple-buffered I2S output bridge for M5Unified speaker
class AudioOutputM5Speaker : public AudioOutput {
public:
  AudioOutputM5Speaker(m5::Speaker_Class* m5sound, uint8_t virtual_sound_channel = 0) {
    _m5sound = m5sound;
    _virtual_ch = virtual_sound_channel;
  }
  virtual ~AudioOutputM5Speaker(void) {}
  virtual bool begin(void) override { return true; }
  virtual bool ConsumeSample(int16_t sample[2]) override {
    if (_tri_buffer_index < tri_buf_size) {
      _tri_buffer[_tri_index][_tri_buffer_index    ] = sample[0];
      _tri_buffer[_tri_index][_tri_buffer_index + 1] = sample[1];
      _tri_buffer_index += 2;
      return true;
    }
    flush();
    return false;
  }
  virtual void flush(void) override {
    if (_tri_buffer_index) {
      _m5sound->playRaw(_tri_buffer[_tri_index], _tri_buffer_index, hertz, true, 1, _virtual_ch);
      _tri_index = _tri_index < 2 ? _tri_index + 1 : 0;
      _tri_buffer_index = 0;
    }
  }
  virtual bool stop(void) override {
    flush();
    _m5sound->stop(_virtual_ch);
    return true;
  }
  const int16_t* getBuffer(void) const { return _tri_buffer[(_tri_index + 2) % 3]; }

protected:
  m5::Speaker_Class* _m5sound;
  uint8_t _virtual_ch;
  static constexpr size_t tri_buf_size = 640;
  int16_t _tri_buffer[3][tri_buf_size];
  size_t _tri_buffer_index = 0;
  size_t _tri_index = 0;
};

// Multi-threaded MP3 audio engine running on Core 0
class AudioDeviceSimple {
private:
  AudioGeneratorMP3*    mp3  = nullptr;
  AudioFileSourceSD*    file = nullptr;
  AudioFileSourceID3*   id3  = nullptr;
  AudioFileSourceBuffer* buff = nullptr;
  AudioOutputM5Speaker* out  = nullptr;
  TaskHandle_t          taskHandle = nullptr;

  char nextSong[100] = "";
  volatile bool triggerPlay = false;
  volatile bool triggerStop = false;

  void cleanup() {
    if (mp3 && mp3->isRunning()) mp3->stop();
    if (buff) { buff->close(); delete buff; buff = nullptr; }
    if (id3)  { id3->close();  delete id3;  id3  = nullptr; }
    if (file) { file->close(); delete file; file = nullptr; }
    isPaused = false;
  }

  static void _audioTask(void* parameter) {
    AudioDeviceSimple* audio = (AudioDeviceSimple*)parameter;
    for (;;) {
      if (audio->triggerPlay) {
        audio->triggerPlay = false;
        audio->cleanup();
        audio->file = new AudioFileSourceSD(audio->nextSong);
        if (audio->file->isOpen()) {
          audio->id3  = new AudioFileSourceID3(audio->file);
          audio->buff = new AudioFileSourceBuffer(audio->id3, 4096);
          if (audio->mp3->begin(audio->buff, audio->out)) {
            audio->isPaused = false;
          } else {
            audio->cleanup();
          }
        } else {
          audio->cleanup();
        }
      }
      if (audio->triggerStop) {
        audio->triggerStop = false;
        audio->cleanup();
      }
      if (audio->mp3 && audio->mp3->isRunning()) {
        if (!audio->isPaused) {
          if (!audio->mp3->loop()) audio->cleanup();
          vTaskDelay(pdMS_TO_TICKS(1));
        } else {
          vTaskDelay(pdMS_TO_TICKS(20));
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(50));
      }
    }
  }

public:
  volatile bool isPaused = false;

  void begin() {
    out = new AudioOutputM5Speaker(&M5Cardputer.Speaker, 0);
    mp3 = new AudioGeneratorMP3();
    xTaskCreatePinnedToCore(_audioTask, "AudioTask", 32768, this, 2, &taskHandle, 0);
  }

  void play(String path) {
    path.trim();
    strncpy(nextSong, path.c_str(), 99);
    nextSong[99] = '\0';
    triggerPlay = true;
  }

  void stop()                    { triggerStop = true; }
  void togglePause()             { if (isPlaying()) isPaused = !isPaused; }
  void setVolume(uint8_t vol)    { M5Cardputer.Speaker.setVolume(vol); }
  bool isPlaying()               { return (mp3 && mp3->isRunning()); }
};
