# Robo explorador espacial — Camada de Serviço

Projeto da Camada de Serviço. A API Python e o SQLite implementam a **Etapa 03**. A branch `feat/robo-lab` organiza os arquivos da **Etapa 04**. O firmware físico envia leituras para a API local; o controle de comandos continua usando Firebase conforme o código de teste da equipe.

## Estrutura

- `backend/app.py`: API Flask com `POST /leituras`, `GET /leituras` e `GET /health`.
- `firmware/robo_explorador.ino`: firmware físico compartilhado para o ESP32-S3 (sensores, motores, Firebase e CallmeBot).
- `firmware/secrets.example.h`: modelo de configuracao local do firmware. Copie para `firmware/secrets.h` e preencha os valores.
- `sql/schema.sql`: estrutura da tabela `leituras`.
- `backend/leituras.db`: banco local criado ao iniciar a API; nao e versionado.

## Firmware do robo

O firmware consulta comandos pelo Firebase e envia as leituras dos sensores a cada 2 segundos para `POST /leituras`. A API deve responder HTTP `201` para confirmar o armazenamento. Se houver falha, o ESP32 mantém até 10 leituras em uma fila na memória e tenta reenviar uma por ciclo de sensores. Se a energia do ESP32 for interrompida, essa fila temporária é perdida.

O codigo de teste usa um HC-SR04 como indicador de presenca, enquanto o edital especifica um sensor PIR. Confirme com a equipe qual componente sera usado e atualize o firmware/README se necessario.

Antes de compilar, copie `firmware/secrets.example.h` para `firmware/secrets.h` e substitua os valores de exemplo, inclusive `BACKEND_URL`. Para o ESP32 físico, use `http://<IP_DO_COMPUTADOR>:5000/leituras`, sem espaços, onde o IP é o IPv4 do computador na mesma rede Wi-Fi do robô. Inicie a API no computador e permita conexões de entrada na porta 5000 no firewall, se solicitado. Instale na Arduino IDE as bibliotecas `DHTesp` e `UrlEncode`, selecione a placa ESP32-S3 usada no laboratorio e confira a pinagem com o circuito real. O `secrets.h` fica ignorado pelo Git; nunca coloque senhas ou chaves nesse arquivo de exemplo.

O endereço privado do computador só funciona para dispositivos que conseguem alcançar essa rede. O Wokwi não consegue acessar diretamente esse IP privado; use o robô físico na rede local para validar esta integração. O ESP32 não se conecta ao Supabase: o caminho exigido nesta implementação é ESP32 → Flask → SQLite.

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

O código imprime no Monitor Serial o endereço configurado, o status HTTP e o corpo de resposta. `201` significa que a API confirmou o registro; outros códigos devem ser tratados como falha e a amostra fica na fila.

## Etapa 04 — GitHub

A branch de trabalho e `feat/robo-lab`. Ela contem o firmware, o backend Python, o esquema SQL e este README. Para publicar novos commits nessa branch:

```powershell
git add README.md backend firmware sql requirements.txt .gitignore
git commit -m "Atualiza projeto do robo explorador"
git push -u origin feat/robo-lab
```

O banco SQLite e as configuracoes locais nao devem ser adicionados ao Git.
