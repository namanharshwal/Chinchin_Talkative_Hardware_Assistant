import requests

invoke_url = "https://integrate.api.nvidia.com/v1/chat/completions"
stream = False

headers = {
    "Authorization": "Bearer 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",
    "Accept": "text/event-stream" if stream else "application/json",
}

payload = {
  "model": "minimaxai/minimax-m3",
  "messages": [
    {
      "role": "user",
      "content": "Which number is larger, 9.11 or 9.8?"
    }
  ],
  "temperature": 1,
  "top_p": 0.95,
  "max_tokens": 8192,
  "stream": stream
}

response = requests.post(invoke_url, headers=headers, json=payload, stream=stream)
if stream:
    for line in response.iter_lines():
        if line:
            print(line.decode("utf-8"))
else:
    print(response.json())
