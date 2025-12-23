 ![Screenshot_27](https://github.com/user-attachments/assets/f3f79af3-42a4-4663-b1d0-612e7d15b11d)
 
 [Read this in English (Inglês)](README.md)

# Plugin Nightbot para OBS Studio

## Introdução

Este é um plugin para o OBS Studio que integra o sistema de *Song Request* (pedidos de música) do Nightbot diretamente na interface do OBS. Ele adiciona um novo painel (Dock) que permite visualizar e gerenciar a fila de músicas pedidas pelos espectadores da sua transmissão na Twitch, sem a necessidade de alternar para o navegador.

## Funcionalidades

*   **Visualização da Fila:** Veja a lista de músicas atualmente na fila do Nightbot.
*   **Controle de Reprodução:** Controle a música atual (tocar, pausar, pular).
*   **Gerenciamento da Fila:** Promova músicas para o topo da lista ou remova-as.
*   **Integração Total:** Tudo é feito dentro de um painel acoplável no OBS Studio.

## Instalação

1.  Baixe o instalador (`.exe`) da página de Releases.
2.  Execute o instalador. Ele tentará localizar automaticamente sua pasta de instalação do OBS Studio.
3.  Após a instalação, inicie o OBS Studio.

## Como Usar

1.  Com o OBS Studio aberto, vá ao menu **Painéis** (ou **Docks**).
2.  Clique na opção **"Nightbot"** para ativar o painel do plugin.
3.  Um novo painel aparecerá. Posicione-o onde preferir na interface do OBS.
4.  Faça login com sua conta do Nightbot para carregar e gerenciar sua fila de músicas.

## Compilando a partir do código-fonte

Se você deseja compilar o plugin manualmente, siga os passos abaixo.

### Requisitos
*   Git
*   CMake (versão 3.28 ou superior)
*   Visual Studio 2022 (com o workload "Desenvolvimento para desktop com C++")
*   Dependências do OBS Studio

### Passos
1.  Clone o repositório:
    `git clone --recursive https://github.com/FabioZumbi12/Nightbot-ObsPlugin.git`
2.  Crie uma pasta para a compilação: `mkdir build && cd build`
3.  Execute o CMake para gerar os arquivos do projeto: `cmake ..`
4.  Compile o projeto usando o Visual Studio ou diretamente pela linha de comando: `cmake --build . --config RelWithDebInfo`

## Contribuições

Contribuições são bem-vindas! Sinta-se à vontade para abrir uma *issue* para relatar problemas ou sugerir novas funcionalidades, ou enviar um *pull request* com melhorias.
