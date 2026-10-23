# Robo explorador espacial — Camada de Serviço

Projeto da Camada de Serviço. A API e o SQLite da **Etapa 03** estao implementados. A branch `feat/robo-lab` organiza os arquivos da **Etapa 04**. A integracao entre o firmware e a API segue pendente enquanto a comunicacao da Etapa 02 e ajustada.

## Estrutura

- `backend/app.py`: API Flask com `POST /leituras`, `GET /leituras` e `GET /health`.
- `firmware/robo_explorador.ino`: firmware físico compartilhado para o ESP32-S3 (sensores, motores, Firebase e CallmeBot).
- `firmware/secrets.example.h`: modelo de configuracao local do firmware. Copie para `firmware/secrets.h` e preencha os valores.
- `sql/schema.sql`: estrutura da tabela `leituras`.
- `backend/leituras.db`: banco local criado ao iniciar a API; nao e versionado.

## Firmware do robo

O firmware foi adicionado para versionar o codigo de teste da Etapa 02. Ele ainda consulta comandos pelo Firebase e nao envia as leituras para `POST /leituras`. A rotina de envio a cada 2 segundos, o buffer/reenvio e a consulta de confirmacao devem ser integrados depois que a conexao com a API for resolvida.

O codigo de teste usa um HC-SR04 como indicador de presenca, enquanto o edital especifica um sensor PIR. Confirme com a equipe qual componente sera usado e atualize o firmware/README se necessario.

Antes de compilar, copie `firmware/secrets.example.h` para `firmware/secrets.h` e substitua os valores de exemplo. Instale na Arduino IDE as bibliotecas `DHTesp` e `UrlEncode`, selecione a placa ESP32-S3 usada no laboratorio e confira a pinagem com o circuito real. O `secrets.h` fica ignorado pelo Git; nunca coloque senhas ou chaves nesse arquivo de exemplo.

O arquivo fornecido continha credenciais reais. Elas foram removidas do firmware versionado; troque a senha Wi-Fi e regenere a chave do CallmeBot antes de continuar usando-as.

## Iniciar o backend

Requer Python 3.10 ou superior.

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python backend/app.py
```

Por padrao, a API fica disponivel na porta 5000 e cria o banco em `backend/leituras.db`. Para escolher outro arquivo, defina `LEITURAS_DB`; para escolher outra porta, defina `PORT`.

## Enviar uma leitura

O ESP32 deve enviar uma requisicao HTTP `POST` para `http://<IP_DO_COMPUTADOR>:5000/leituras`, com `Content-Type: application/json`. Use o IP do computador na rede local, nao `localhost`, no firmware do ESP32.

```json
{
  "timestamp": "2025-09-02T14:35:00Z",
  "temperatura_c": 24.3,
  "umidade_pct": 55,
  "luminosidade": 723,
  "presenca": 1,
  "probabilidade_vida": 78.0
}
```

`timestamp` e opcional; se omitido, o servidor registra o horario UTC atual. `presenca` deve ser `0` (sem presenca) ou `1` (presenca detectada). Uma gravacao bem-sucedida retorna HTTP `201` com o identificador da leitura. Dados invalidos retornam HTTP `400` e uma mensagem no campo `erro`.

## Consultar leituras

Abra `http://<IP_DO_COMPUTADOR>:5000/leituras` ou execute:

```powershell
Invoke-RestMethod http://127.0.0.1:5000/leituras
```

A resposta e uma lista JSON ordenada da mais recente para a mais antiga, limitada a 100 registros. O endpoint `GET /health` verifica se a API esta ativa.

## Envio a cada dois segundos e falhas de rede

No firmware da Etapa 02, monte o JSON com as leituras atualizadas a cada 2 segundos e considere a amostra entregue somente quando receber HTTP `201`. Se a conexao ou a requisicao falhar, mantenha a amostra em uma fila em memoria e tente reenviar quando a rede voltar; remova-a da fila somente apos resposta `201`. Limite a fila para evitar consumo ilimitado de memoria e registre no Monitor Serial as falhas e os reenvios. O endereco deve apontar para o computador que executa esta API e ambos os dispositivos precisam ter conectividade entre si.

Quando a integracao for concluida, atualize esta secao com o endereço da API acessível pelo ESP32 e os passos de teste na rede do laboratorio.

## Etapa 04 — GitHub

A branch de trabalho e `feat/robo-lab`. Ela contem o firmware, o backend Python, o esquema SQL e este README. Para publicar novos commits nessa branch:

```powershell
git add README.md backend firmware sql requirements.txt .gitignore
git commit -m "Atualiza projeto do robo explorador"
git push -u origin feat/robo-lab
```

O banco SQLite e as configuracoes locais nao devem ser adicionados ao Git.
