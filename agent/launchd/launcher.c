/*
 * Executável do MacMonitorAgent.app: só troca de processo para o comando
 * recebido (o Python do venv rodando o agent.py).
 *
 * Existe por causa do Bluetooth: sob o launchd o processo "responsável" perante
 * o TCC é o primeiro executável lançado, e o python3 da Apple é binário de
 * plataforma, para o qual o macOS nega o Bluetooth sem nem perguntar. Com este
 * app na frente, a permissão é pedida em nome dele (Info.plist com
 * NSBluetoothAlwaysUsageDescription) e o exec mantém essa responsabilidade.
 */
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "uso: %s <programa> [argumentos...]\n", argv[0]);
        return 64;
    }
    execv(argv[1], argv + 1);
    perror(argv[1]);
    return 127;
}
