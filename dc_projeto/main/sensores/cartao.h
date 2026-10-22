#ifndef CARTAO_H
#define CARTAO_H

#include <esp_err.h>
#include <stdbool.h>

#define FORMAT_VERSION 1

// Inicializa a interface com o cartão microSD e o sistema de ficheiros VFS FAT
esp_err_t cartao_init(void);

// Desmonta o sistema de ficheiros
esp_err_t cartao_deinit(void);

// RF4: Registo de leitura de temperatura e humidade
esp_err_t cartao_registar_leitura(float temperatura, float humidade);

// RF4: Registo de log de operações do sistema
esp_err_t cartao_registar_log(const char *nivel, const char *mensagem);

#endif 