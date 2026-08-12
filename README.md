# SOFIA Firmware - Guia de Build e Deploy (Raspberry Pi)

Este guia explica como transferir, compilar e executar o firmware SOFIA na sua Raspberry Pi 3B a partir de um computador Windows.

## 1. Preparação da Raspberry Pi

Antes de compilar, você precisa garantir que as ferramentas de desenvolvimento e o barramento I2C estejam prontos.

### Dependências
Conectado via SSH (PuTTY), execute:
```bash
sudo apt update
sudo apt install -y build-essential cmake
```

### Habilitar I2C
O PCF8591 usa o barramento I2C. Se ainda não habilitou:
1. Execute `sudo raspi-config`.
2. Vá em **Interface Options** -> **I2C**.
3. Selecione **Yes** para habilitar.
4. Reinicie se solicitado ou execute `sudo modprobe i2c-dev`.

---

## 2. Transferindo os Arquivos (Windows -> Pi)

Como você está no Windows usando PuTTY, aqui estão as duas melhores formas:

### Opção A: WinSCP (Recomendado - Interface Gráfica)
1. Instale o [WinSCP](https://winscp.net/).
2. Conecte no IP da sua Pi (mesmas credenciais do PuTTY).
3. Arraste a pasta `sofia-firmware` do Windows para dentro da pasta `/home/pi` (ou sua home).

### Opção B: PSCP (Linha de comando via PuTTY)
O PuTTY vem com uma ferramenta chamada `pscp.exe`. No CMD ou PowerShell do seu Windows:
```powershell
# Exemplo de comando:
pscp -r .\sofia-firmware sofiapi@IP_DA_RASPBERRY:/home/sofiapi/
```

---

## 3. Configuração do Código

### FreeRTOS (.disabledc)
Como estamos compilando para Linux (Raspbian), o arquivo do FreeRTOS pode causar erros se estiver usando um editor que tente indexar os arquivos do projeto.
- Pode renomear `src/osal/freertos/osal_thread_freertos.c` para `osal_thread_freertos.disabledc`.
- O `CMakeLists.txt` já está configurado para ignorar esse arquivo por padrão, mas renomear evita que IDEs tentem indexá-lo erroneamente.

---

## 4. Compilação e Execução

Dentro da Raspberry Pi (via PuTTY):

```bash
cd ~/sofia-firmware
mkdir build
cd build

# Gera os arquivos de build (Configurado para POSIX/Linux por padrão)
cmake ..

# Compila o projeto
make

# Executa o Firmware (sudo é necessário para acessar o barramento I2C /dev/i2c-1)
sudo ./sofia_firmware
```

## 5. Comandos do Teste
- **Ctrl+C**: Encerramento gracioso das threads.
- **F**: Mostra "Respect Paid" no terminal.
- **Z / A / Q**: Muda a forma de onda enviada ao DAC (AOUT).
- **Toque no AN0**: Observe os valores de interferência subindo no terminal.
