#include "cartao.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "CARTAO_SD";

#define MOUNT_POINT "/sdcard"
#define PATH_LEITURAS MOUNT_POINT "/readings.csv"
#define PATH_LOGS     MOUNT_POINT "/system.log"

// Pinos SPI do ESP32-C6 
#define PIN_NUM_MISO 18
#define PIN_NUM_MOSI 19
#define PIN_NUM_CLK  20
#define PIN_NUM_CS   21

static sdmmc_card_t *s_card = NULL;
static bool s_mounted = false;

esp_err_t cartao_init(void) {
    if (s_mounted) {
        ESP_LOGW(TAG, "Cartão SD já se encontra montado.");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "A inicializar o cartão SD via SDSPI...");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o barramento SPI (%s)", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI2_HOST;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Falha ao montar o sistema de ficheiros FAT.");
        } else {
            ESP_LOGE(TAG, "Falha ao inicializar o cartão SD (%s).", esp_err_to_name(ret));
        }
        return ret;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "Cartão SD montado com sucesso em " MOUNT_POINT);

    
    struct stat st;
    if (stat(PATH_LEITURAS, &st) != 0) {
        FILE *f = fopen(PATH_LEITURAS, "w");
        if (f != NULL) {
            fprintf(f, "# format_version:%d\n", FORMAT_VERSION);
            fprintf(f, "timestamp,temperatura,humidade\n");
            fclose(f);
        }
    }

    return ESP_OK;
}

esp_err_t cartao_deinit(void) {
    if (!s_mounted) {
        return ESP_OK;
    }
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
    spi_bus_free(SPI2_HOST);
    s_mounted = false;
    s_card = NULL;
    ESP_LOGI(TAG, "Cartão SD desmontado.");
    return ret;
}

esp_err_t cartao_registar_leitura(float temperatura, float humidade) {
    if (!s_mounted) {
        ESP_LOGE(TAG, "Erro: Cartão SD não montado. Não foi possível guardar leitura.");
        return ESP_ERR_INVALID_STATE;
    }

    FILE *f = fopen(PATH_LEITURAS, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir o ficheiro %s para escrita.", PATH_LEITURAS);
        return ESP_FAIL;
    }

    time_t now;
    time(&now);

    // Registo do timestamp Unix juntamente com as leituras
    fprintf(f, "%lld,%.2f,%.2f\n", (long long)now, temperatura, humidade);
    fclose(f);

    ESP_LOGD(TAG, "Leitura registada: Temp=%.2f, Hum=%.2f", temperatura, humidade);
    return ESP_OK;
}

esp_err_t cartao_registar_log(const char *nivel, const char *mensagem) {
    if (!s_mounted) {
        ESP_LOGE(TAG, "Erro: Cartão SD não montado. Não foi possível guardar o log.");
        return ESP_ERR_INVALID_STATE;
    }

    // RNF6: Garantir que credenciais/palavras-passe não são gravadas nos logs
    if (strstr(mensagem, "password") != NULL || strstr(mensagem, "pass") != NULL) {
        ESP_LOGW(TAG, "Tentativa de registo de mensagem com dados sensíveis bloqueada.");
        mensagem = "[REDACTED_SENSITIVE_DATA]";
    }

    FILE *f = fopen(PATH_LOGS, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir o ficheiro %s para escrita de log.", PATH_LOGS);
        return ESP_FAIL;
    }

    time_t now;
    time(&now);

    fprintf(f, "[v%d][%lld][%s] %s\n", FORMAT_VERSION, (long long)now, nivel, mensagem);
    fclose(f);

    return ESP_OK;
}