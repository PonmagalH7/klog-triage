# klog-triage

A Linux kernel log triage tool written in C. It reads kernel messages, keeps only the important ones, queues them by priority, and uses a local AI model (Ollama) to explain each error in plain English.

## How it works

```
/dev/kmsg  (or a replay file)
     |
     v
 [Reader thread]  -- parses each line, keeps kernel levels 0-4 only
     |
     v
 [Priority queue] -- most severe first, with aging so old events are not starved
     |
     v
 [Worker threads] -- take events, ask the LLM to explain, print the result
     |
     v
 [Ollama + qwen2.5:3b]  -- falls back to a built-in explanation if unavailable
```

### Components

| File | What it does |
|------|--------------|
| `src/main.c` | Starts the reader and workers, handles Ctrl+C, shuts everything down cleanly |
| `src/reader.c` | Reads `/dev/kmsg` (live) or a replay file, parses events, pushes them to the queue |
| `src/triage.c` | `should_triage()` keeps only levels 0-4 (emergency to warning) |
| `src/queue.c` | Thread-safe bounded priority queue (100 slots) with aging |
| `src/worker.c` | Worker threads: pop an event, call the LLM, print the explanation |
| `src/llm.c` | HTTP client for Ollama via libcurl, with retry and fallback explanations |
| `demo_events.txt` | Sample kernel events for testing without root |
| `tests/` | Standalone tests for the queue, triage filter, concurrency, aging and shutdown |

### Kernel log levels

| Level | Meaning | Triaged? |
|-------|---------|----------|
| 0 | Emergency | Yes |
| 1 | Alert | Yes |
| 2 | Critical | Yes |
| 3 | Error | Yes |
| 4 | Warning | Yes |
| 5-7 | Notice, Info, Debug | No (ignored) |

### Priority and aging

Lower level number = higher priority. Every `AGING_INTERVAL_SECONDS` an event waits, its effective priority improves by one level, so a low-priority event can never wait forever. Ties are broken by arrival time (oldest first).

---

## Step-by-step setup

Tested on Ubuntu under WSL2. Any modern Linux should work.

### Step 1: Install build tools and libcurl

```bash
sudo apt update
sudo apt install -y build-essential libcurl4-openssl-dev
```

### Step 2: Install Ollama

Ollama needs `zstd` to extract itself:

```bash
sudo apt-get install -y zstd
curl -fsSL https://ollama.com/install.sh | sh
```

Check it installed:

```bash
ollama --version
```

### Step 3: Download the model

```bash
ollama pull qwen2.5:3b
```

This is about 1.9 GB.

### Step 4: Make sure Ollama is running

The installer normally starts it as a service. Check:

```bash
curl -s localhost:11434/api/tags
```

If you see JSON listing `qwen2.5:3b`, you are good.

If you run `ollama serve` and get `address already in use`, that just means it is already running. Nothing more to do.

If nothing responds, start it in a separate terminal:

```bash
ollama serve
```

### Step 5: Get the code and build

```bash
git clone https://github.com/PonmagalH7/klog-triage.git
cd klog-triage
make
```

The build runs:

```bash
gcc -Wall -Wextra -O2 -pthread src/main.c src/reader.c src/worker.c src/queue.c src/triage.c src/llm.c -o klog-triage -lcurl
```

### Step 6: Run the demo (no root needed)

```bash
./klog-triage --replay demo_events.txt
```

You will see events being queued, then each one explained, then `Shut down cleanly.`

Example output:

```
[Reader] queued level=2 seq=5: Out of memory: Killed process 4121 (chrome) ...

[Worker ...] level=2 seq=5
  Event: Out of memory: Killed process 4121 (chrome) ...
What happened: ...
Possible cause: ...
Suggested solution: ...
```

Events are explained in priority order (level 1 before level 2, and so on).

### Step 7: Run on your real kernel log

Reading `/dev/kmsg` needs root:

```bash
sudo ./klog-triage
```

Press **Ctrl+C** to stop. The program finishes cleanly.

> On WSL2, the kernel log may be quiet. Use the replay mode to see the tool working.

---

## Configuration

These are compile-time settings. Edit, then run `make` again.

| Setting | File | Default | What it does |
|---------|------|---------|--------------|
| `WORKER_COUNT` | `src/worker.h` | `1` | Number of worker threads |
| `REQUEST_TIMEOUT` | `src/llm.c` | `180L` | Seconds to wait for the LLM per request |
| `AGING_INTERVAL_SECONDS` | `src/queue.c` | `60` | Seconds of waiting before an event moves up one level |
| `QUEUE_SIZE` | `src/queue.h` | `100` | Maximum events waiting in the queue |
| `OLLAMA_MODEL` | `src/llm.c` | `qwen2.5:3b` | Which Ollama model to use |
| `OLLAMA_URL` | `src/llm.c` | `http://localhost:11434/api/generate` | Where Ollama is listening |

Example:

```bash
sed -i 's/#define WORKER_COUNT 3/#define WORKER_COUNT 1/' src/worker.h
make
```

### Why `WORKER_COUNT` is 1

A 3B model on CPU is slow. With several workers all calling the model at once, requests queue up inside Ollama and hit the timeout. Those events then get the generic fallback text instead of a real explanation. One worker with a 180 second timeout gave a real AI explanation for every event in testing. If you have a GPU or a faster machine, raise it.

---

## Fallback behaviour

If Ollama is not running, times out, or returns an error, `llm.c` retries once and then prints a built-in explanation. It recognises out of memory, I/O errors and segfaults. Anything else gets a generic message telling you to check the kernel logs.

---

## Running the tests

The tests are small standalone programs that print results. From the project root:

```bash
# Triage filter
gcc -Isrc tests/test_triage.c src/triage.c -o test_triage && ./test_triage

# Concurrent producers and consumers (expects 300 of 300 events processed)
gcc -Isrc tests/test_concurrent.c src/queue.c -o test_concurrent -lpthread && ./test_concurrent

# Shutdown while the queue is full
gcc -Isrc tests/test_shutdown.c src/queue.c -o test_shutdown -lpthread && ./test_shutdown

# Aging (takes about 22 seconds because it waits for aging to happen)
gcc -Isrc tests/test_aging.c src/queue.c -o test_aging -lpthread && ./test_aging
```

Note: `test_aging` was written for a 5 second aging interval (it waits 21 seconds). With `AGING_INTERVAL_SECONDS` set to 60, the low-priority event will not be promoted in that time, so change the interval back to 5 temporarily, or raise the sleep in the test, to see the aging result.

`test_llm.c` and `test_llm_10.c` in the root call Ollama directly:

```bash
gcc test_llm.c src/llm.c -o test_llm -lcurl && ./test_llm
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `curl/curl.h: No such file or directory` | `sudo apt install libcurl4-openssl-dev` |
| `open /dev/kmsg (try sudo)` | Run with `sudo`, or use `--replay` |
| Every explanation says "A kernel error was reported" | Ollama is not reachable or timed out. Check `curl localhost:11434/api/tags` and keep `WORKER_COUNT` at 1 |
| `ollama: command not found` | Install `zstd` first, then rerun the Ollama install script |
| `address already in use` on `ollama serve` | Ollama is already running, which is fine |
| First explanation is very slow | The model is loading into memory. Later requests are faster |

---

## Known limitations

- The 3B model can be wrong or vague. For example it may call a user-space segfault a "kernel" fault. Treat explanations as hints, not diagnoses.
- The fallback explanation and a real AI explanation look the same in the output.
- On Ctrl+C, events already in the queue are still processed before exit, which can take a while with a slow model.
- Only Linux is supported.

---

## Requirements

- Linux (or WSL2)
- gcc, make, pthreads
- libcurl development headers
- Ollama with the `qwen2.5:3b` model
- Root access for live `/dev/kmsg` reading
