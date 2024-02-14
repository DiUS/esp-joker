#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_random.h"
#include "model_path.h"

#include "esp-box.h"
#include "picotts.h"

#include "jokes.h"
#include "jokes.ic"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORD_VOLUME 27.0

static int detect_flag = 0;
static esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
static volatile int pause_flag = 0;
static srmodel_list_t *models = NULL;
static esp_afe_sr_data_t *afe_data = NULL;

static esp_codec_dev_handle_t spk_codec;
static esp_codec_dev_handle_t mic_codec;

enum { REBOOT_CMD_ID, JOKE_CMD_ID };


static void on_tts_samples(int16_t *buf, unsigned count)
{
  esp_codec_dev_write(spk_codec, buf, count*2);
}


static void on_tts_idle(void)
{
  printf("Resuming mic input\n");
  pause_flag = 0;
}


static void say(const char *what, unsigned sz)
{
  printf("Pausing mic input\n");
  pause_flag = 1;
  picotts_add(what, sz);
}

void feed_Task(void *arg)
{
  esp_afe_sr_data_t *afe_data = arg;
  int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
  int nch = afe_handle->get_channel_num(afe_data);
  int channels = bsp_audio_get_mic_channels();
  assert(nch <= channels);
  unsigned buff_sz = audio_chunksize * sizeof(int16_t) * channels;
  int16_t *i2s_buff = malloc(buff_sz);
  assert(i2s_buff);

  while (task_flag) {
    if (pause_flag)
    {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    esp_err_t ret = esp_codec_dev_read(mic_codec, i2s_buff, buff_sz);
    if (ret != ESP_OK)
    {
      printf("mic read failed: %d (%s)\n", ret, esp_err_to_name(ret));
      break;
    }
    // I'm confused about this, but okay...
    for (int i = 0; i < audio_chunksize; i++) {
      int16_t ref = i2s_buff[4 * i + 0];
      i2s_buff[3 * i + 0] = i2s_buff[4 * i + 1];
      i2s_buff[3 * i + 1] = i2s_buff[4 * i + 3];
      i2s_buff[3 * i + 2] = ref;
    }
    afe_handle->feed(afe_data, i2s_buff);
  }
  if (i2s_buff) {
    free(i2s_buff);
    i2s_buff = NULL;
  }
  vTaskDelete(NULL);
}

void detect_Task(void *arg)
{
  esp_afe_sr_data_t *afe_data = arg;
  int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
  char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
  printf("multinet:%s\n", mn_name);
  esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
  model_iface_data_t *model_data = multinet->create(mn_name, 6000);
  int mu_chunksize = multinet->get_samp_chunksize(model_data);
  assert(mu_chunksize == afe_chunksize);
  esp_mn_commands_clear();
  esp_mn_commands_add(REBOOT_CMD_ID, "RmSTnRT PLmZ"); // restart please
  esp_mn_commands_add(JOKE_CMD_ID, "TfL Mm c qbK"); // tell me a joke
  esp_mn_commands_add(JOKE_CMD_ID, "TfL Mm cNcjk qbK"); // tell me another joke
  esp_mn_commands_add(JOKE_CMD_ID, "cNcjk qbK"); // another joke
  esp_mn_commands_add(JOKE_CMD_ID, "cMYoZ Mm"); // amuse me
  esp_mn_commands_add(JOKE_CMD_ID, "fNTkTdN Mm"); // entertain me
  esp_mn_commands_update();
  multinet->print_active_speech_commands(model_data);

  printf("------------detect start------------\n");
  while (task_flag) {
    if (pause_flag)
    {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    afe_fetch_result_t* res = afe_handle->fetch(afe_data); 
    if (!res || res->ret_value == ESP_FAIL) {
      printf("fetch error!\n");
      break;
    }

    if (res->wakeup_state == WAKENET_DETECTED) {
      printf("WAKEWORD DETECTED\n");
      multinet->clean(model_data);
    } else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
      detect_flag = 1;
      printf("AFE_FETCH_CHANNEL_VERIFIED, channel index: %d\n", res->trigger_channel_id);
      afe_handle->disable_wakenet(afe_data);
      afe_handle->disable_aec(afe_data);

      static const char resp[] = "Yes?";
      say(resp, sizeof(resp));
    }

    if (detect_flag == 1) {
      esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

      if (mn_state == ESP_MN_STATE_DETECTING)
        continue;

      if (mn_state == ESP_MN_STATE_DETECTED) {
        esp_mn_results_t *mn_result = multinet->get_results(model_data);
        for (int i = 0; i < mn_result->num; i++) {
          printf("TOP %d, command_id: %d, phrase_id: %d, string: %s, prob: %f\n", 
          i+1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
        }
        if (mn_result->num > 0)
        {
          switch (mn_result->command_id[0])
          {
            case REBOOT_CMD_ID:
            {
              const char okay[] = "Okay, restarting";
              say(okay, sizeof(okay));
              vTaskDelay(pdMS_TO_TICKS(2000));
              esp_restart();
              break;
            }
            case JOKE_CMD_ID:
            {
              const unsigned n_jokes = sizeof(jokes)/sizeof(jokes[0]);
              int i = esp_random() % n_jokes;
              printf("Reciting joke %d/%d\n", i, n_jokes);
              say(jokes[i], strlen(jokes[i]) + 1);
              break;
            }
          }
        }
        printf("-----------listening-----------\n");
      }

      if (mn_state == ESP_MN_STATE_TIMEOUT) {
          esp_mn_results_t *mn_result = multinet->get_results(model_data);
        printf("timeout, string:%s\n", mn_result->string);
        afe_handle->enable_wakenet(afe_data);
        detect_flag = 0;
        printf("\n-----------awaits to be waken up-----------\n");
        continue;
      }
    }
  }
  if (model_data) {
    multinet->destroy(model_data);
    model_data = NULL;
  }
  printf("detect exit\n");
  vTaskDelete(NULL);
}

void app_main()
{
  ESP_ERROR_CHECK(bsp_i2c_init());
  ESP_ERROR_CHECK(bsp_audio_init(NULL));

  spk_codec = bsp_audio_codec_speaker_init();
  assert(spk_codec);
  esp_codec_dev_set_out_vol(spk_codec, 70);
  esp_codec_dev_sample_info_t fs = {
    .sample_rate = PICOTTS_SAMPLE_FREQ_HZ,
    .channel = 1,
    .bits_per_sample = PICOTTS_SAMPLE_BITS,
  };
  esp_codec_dev_open(spk_codec, &fs);

  mic_codec = bsp_audio_codec_microphone_init();
  assert(mic_codec);
  fs.channel = 2;
  fs.bits_per_sample = 32;
  esp_codec_dev_open(mic_codec, &fs);
  esp_codec_dev_set_in_channel_gain(
    mic_codec, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), RECORD_VOLUME);
  esp_codec_dev_set_in_channel_gain(
    mic_codec, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1), RECORD_VOLUME);
  esp_codec_dev_set_in_channel_gain(
    mic_codec, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2), 0);
  esp_codec_dev_set_in_channel_gain(
    mic_codec, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(3), RECORD_VOLUME);

  models = esp_srmodel_init("model"); // partition label defined in partitions.csv

  afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
  afe_config_t afe_config = AFE_CONFIG_DEFAULT();
  afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);;
  afe_config.aec_init = false;
  afe_data = afe_handle->create_from_config(&afe_config);


  task_flag = 1;
  xTaskCreatePinnedToCore(
    &detect_Task, "detect", 8 * 1024, (void*)afe_data, 5, NULL, 1);

  xTaskCreatePinnedToCore(
    &feed_Task, "feed", 8 * 1024, (void*)afe_data, 5, NULL, 0);


  if (!picotts_init(5, on_tts_samples, 1))
  {
    printf("Failed to initialise TTS\n");
    task_flag = 0;
  }
  else
  {
    picotts_set_idle_notify(on_tts_idle);

    static const char hello[] = "Hi, I'm Willow;; I can tell terrible jokes;";
    say(hello, sizeof(hello));
  }

  // printf("destroy\n");
  // afe_handle->destroy(afe_data);
  // afe_data = NULL;
  // printf("successful\n");
}
