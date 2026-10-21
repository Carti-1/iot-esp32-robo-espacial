# Armazenamento de dados do robo explorador

Implementacao da **Etapa 03** do edital: API HTTP em Python, banco SQLite, registro periodico das leituras dos sensores e consulta das 100 leituras mais recentes.

## Estrutura

- `backend/app.py`: API Flask com `POST /leituras`, `GET /leituras` e `GET /health`.
- `backend/leituras.db`: banco criado automaticamente ao iniciar a API.
- `sql/schema.sql`: estrutura da tabela `leituras`.

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

O firmware da Etapa 02 ainda nao esta nesta pasta; por isso, a inclusao do POST e da fila de reenvio deve ser feita no codigo do ESP32 quando o colega compartilhar essa parte.
