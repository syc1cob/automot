# Mini Diagnostics API

A small C++14 REST API using oat++ 1.3.x and simulated vehicle data. This is a learning project.

## Files

- [CMakeLists.txt](CMakeLists.txt): build configuration.
- [include/ecu_dto.hpp](include/ecu_dto.hpp): DTOs that define ECU and error JSON shapes.
- [include/diagnostics_controller.hpp](include/diagnostics_controller.hpp): routes, validation, and database error responses.
- [include/ecu_repository.hpp](include/ecu_repository.hpp): interface for ECU persistence operations.
- [src/ecu_repository.cpp](src/ecu_repository.cpp): SQLite schema, prepared statements, and data access.
- [src/main.cpp](src/main.cpp): server startup and wiring.
- [tests/api_tests.py](tests/api_tests.py): automated HTTP integration tests, including restart persistence.
- [openapi.yaml](openapi.yaml): OpenAPI 3.0.3 description of the REST API.

## Build

Requirements: CMake 3.22+, a C++14 compiler, oat++ 1.3.x, SQLite 3 development files, and Python 3 for the API tests. SQLite is open-source, public-domain software and runs as a local database file; no separate database server is needed.

The SQLite runtime library is needed to run the API; the development package is
needed to compile it. On Ubuntu or WSL, install both if they are not present:

```sh
sudo apt-get update
sudo apt-get install libsqlite3-0 libsqlite3-dev
```

From this directory:

```sh
cmake -S . -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DOATPP_SOURCE_DIR="$(cd ../external/oatpp-1.3.0 && pwd)"
cmake --build build --parallel 2
python3 tests/api_tests.py ./build/mini-diagnostics-api
```

If you move the project, pass the absolute path to your oat++ source instead. Alternatively, with an installed oat++ package, omit `OATPP_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to its installation prefix if necessary. Use a fresh build directory when switching dependency sources.

## API documentation

[openapi.yaml](openapi.yaml) describes the endpoints, request and response schemas, status codes, and example payloads. Open it in an OpenAPI viewer or import it into an API client that supports OpenAPI to explore the API interactively.

## Run

```sh
./build/mini-diagnostics-api
```

The default port is `8080`, and the default SQLite database file is `ecus.db` in the current working directory. To choose both a different port and database file, pass them as arguments, for example `./build/mini-diagnostics-api 18080 ./demo.db`. If you choose a path inside a directory, that directory must already exist. To start the manual walkthrough with fresh sample data, stop the server and remove the selected database file first.

The server listens only on `127.0.0.1` for local learning. Stop it with Ctrl+C. There is no authentication or TLS; do not expose it as a production service. ECU data is persisted in SQLite and remains available after restarting the server. The sample engine and battery ECUs are inserted only when the database is empty.

## Phase 1: list ECUs

On first startup with a new or empty database, the API seeds the engine and battery ECUs. Later runs reuse the saved records instead of resetting them.

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
- API errors use a JSON body such as `{"error":"ECU not found"}`. Database failures return `500 Internal Server Error`; details are logged by the server rather than returned to the client.

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

Created data is saved in the configured SQLite file and remains after the server restarts.

## Phase 3: replace and delete ECU resources

`PUT` replaces an existing ECU. The ID in the URL must match the ID in the JSON body. A valid replacement returns `200 OK`; invalid input returns `400 Bad Request`, and a resource that does not exist returns `404 Not Found`.

```sh
curl -i -X PUT http://127.0.0.1:18080/api/v1/ecus/engine \
  -H 'Content-Type: application/json' \
  -d '{"id":"engine","name":"Updated Engine Controller","status":"offline","firmwareVersion":"2.0.0"}'
```

`DELETE` removes an existing ECU. Success returns `204 No Content` with no body; deleting an unknown ID returns `404 Not Found`.

```sh
curl -i -X DELETE http://127.0.0.1:18080/api/v1/ecus/engine
```

These operations update the same SQLite database used by `GET` and `POST`. Changes remain after the server restarts.

### Example responses

A successful `GET /api/v1/ecus/engine` returns `200 OK` with:

```json
{
  "id": "engine",
  "name": "Engine Controller",
  "status": "online",
  "firmwareVersion": "1.0.0"
}
```

An unknown ECU returns `404 Not Found` with:

```json
{"error":"ECU not found"}
```

## Automated tests

The integration tests launch the API on an available local port with a separate temporary SQLite file for each test. They cover listing, retrieving, creating, replacing, and deleting ECUs, validation, not-found/conflict responses, and persistence across a server restart.

```sh
python3 tests/api_tests.py ./build/mini-diagnostics-api
```

The tests are also registered with CTest and can be run from a CMake-aware IDE. The generated database file is ignored by Git; do not commit local runtime data.

## Project structure

- `include/ecu_dto.hpp` describes the JSON data types.
- `include/diagnostics_controller.hpp` contains the API routes and request validation.
- `include/ecu_repository.hpp` and `src/ecu_repository.cpp` isolate the SQLite storage operations.
- `src/main.cpp` initializes oat++ and wires the server components together.
- `tests/api_tests.py` verifies the public HTTP behavior without needing another test framework.

## What this demonstrates

- `GET` reads the collection or an individual resource.
- `POST` creates a resource from a JSON request body.
- `PUT` replaces an existing resource; `DELETE` removes one.
- Path parameters identify a resource; the request body carries new resource data.
- A SQLite repository keeps endpoint behavior consistent and persists data across server restarts.
- HTTP status codes communicate success and failure.