# Mini Diagnostics API

A small C++14 REST API using oat++ 1.3.x and simulated vehicle data. This is a learning project, not a SOVD-compliant implementation. 

## Files

- [CMakeLists.txt](CMakeLists.txt): build configuration.
- [src/main.cpp](src/main.cpp): ECU and error DTOs, in-memory data, HTTP endpoints, and server startup.

## Build

Requirements: CMake 3.22+, a C++14 compiler, and oat++ 1.3.x. No database, real vehicle, or network download is needed when using the oat++ source already available in this workspace.

From this directory:

```sh
cmake -S . -B build -DOATPP_SOURCE_DIR="$(cd ../external/oatpp && pwd)"
cmake --build build --parallel 2
```

If you move the project, pass the absolute path to your oat++ source instead. Alternatively, with an installed oat++ package, omit `OATPP_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to its installation prefix if necessary. Use a fresh build directory when switching dependency sources.

## Run

```sh
./build/mini-diagnostics-api
```

The default port is `8080`. To choose a different port if it is already in use, pass it as an argument, for example `./build/mini-diagnostics-api 18080`.

The server listens only on `127.0.0.1` for local learning (port `8080` by default). Stop it with Ctrl+C. There is no authentication or TLS; do not expose it as a production service. ECU data is held in memory and resets when the server restarts.

## Phase 1: list ECUs

```sh
curl -i http://127.0.0.1:8080/api/v1/ecus
```

Expected: HTTP `200 OK` and a JSON array. The initial list contains the engine and battery ECUs.

## Phase 2: read and create ECU resources

### Read one ECU

```sh
curl -i http://127.0.0.1:8080/api/v1/ecus/engine
```

An unknown ID returns `404 Not Found`:

```sh
curl -i http://127.0.0.1:8080/api/v1/ecus/not-real
```

### Create an ECU

```sh
curl -i -X POST http://127.0.0.1:8080/api/v1/ecus \
  -H 'Content-Type: application/json' \
  -d '{"id":"brake","name":"Brake Controller","status":"online","firmwareVersion":"1.0.0"}'
```

A successful create returns `201 Created`, the created ECU as JSON, and a `Location` header. The new ECU will appear in the list and can be retrieved by ID.

Validation requires non-empty `id`, `name`, and `status`. IDs may use letters, digits, hyphens, and underscores. Status must be `online`, `offline`, or `unknown`.

- Invalid or missing data returns `400 Bad Request`.
- Reusing an existing ID returns `409 Conflict`.
- API errors use a JSON body such as `{"error":"ECU not found"}`.

### Step-by-step Phase 2 walkthrough

If port `8080` is already in use, start this project on another port in its own terminal:

```sh
./build/mini-diagnostics-api 18080
```

Use `18080` in the following examples. If you started the server on the default port instead, replace `18080` with `8080`.

1. **List the current ECU resources** — this should show the engine and battery ECUs:

   ```sh
   curl -i http://127.0.0.1:18080/api/v1/ecus
   ```

2. **Read one ECU by its ID** — this should return the engine ECU:

   ```sh
   curl -i http://127.0.0.1:18080/api/v1/ecus/engine
   ```

3. **Create a simulated ECU** — expect `201 Created` and a `Location: /api/v1/ecus/brake` response header:

   ```sh
   curl -i -X POST http://127.0.0.1:18080/api/v1/ecus \
     -H 'Content-Type: application/json' \
     -d '{"id":"brake","name":"Brake Controller","status":"online","firmwareVersion":"1.0.0"}'
   ```

4. **Confirm the new resource is stored** — both commands should now return the brake ECU:

   ```sh
   curl -i http://127.0.0.1:18080/api/v1/ecus/brake
   curl -i http://127.0.0.1:18080/api/v1/ecus
   ```

5. **Try error cases** — a missing ID returns `404`; repeating the create returns `409` because `brake` already exists:

   ```sh
   curl -i http://127.0.0.1:18080/api/v1/ecus/not-real
   curl -i -X POST http://127.0.0.1:18080/api/v1/ecus \
     -H 'Content-Type: application/json' \
     -d '{"id":"brake","name":"Brake Controller","status":"online"}'
   ```

Created data is held in memory, so it is available while this server process is running and is reset when the server restarts.

## What this demonstrates

- `GET` reads the collection or an individual resource.
- `POST` creates a resource from a JSON request body.
- Path parameters identify a resource; the request body carries new resource data.
- The in-memory map keeps endpoints working against the same collection.
- HTTP status codes communicate success and failure.