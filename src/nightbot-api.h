#ifndef NIGHTBOT_API_H
#define NIGHTBOT_API_H

#include <string>

class NightbotAPI {
public:
	static NightbotAPI &get();

	// Busca as informações do usuário autenticado.
	// Retorna o nome de exibição do usuário em caso de sucesso, ou uma string vazia em caso de falha.
	std::string FetchUserInfo();

private:
	NightbotAPI();
};

#endif // NIGHTBOT_API_H