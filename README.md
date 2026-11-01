# relay

Небольшой HTTP/1.1 сервер на C++20: статика с диска и reverse proxy на backend. Работает на Windows и Linux, без nginx и без Python.

Не production-ready в смысле nginx — нет TLS, HTTP/2 и нормального кэша. Зато код читаемый: парсер HTTP, роутинг по `Host`, отдача файлов с проверкой пути, прокси через отдельное TCP-соединение.

## Возможности

- несколько сайтов на одном порту (`Host` → папка или upstream);
- `GET`/`HEAD`, keep-alive для простых GET;
- лимиты на размер заголовков и тела;
- защита от `..` при раздаче статики;
- конфиг в TOML-подобном формате.

## Сборка

Нужны CMake 3.20+, компилтор с C++20 и Git (Asio подтягивается через FetchContent).

**Windows (Visual Studio или Build Tools):**

```powershell
cd relay
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Бинарник: `build\Release\relay.exe` (или `build\relay.exe` с Ninja).

**Linux:**

```bash
cd relay
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/relay run
```

## Быстрый старт

```bash
cp config/relay.example.toml relay.toml   # Windows: copy config\relay.example.toml relay.toml
relay check
relay run
```

Открой [http://127.0.0.1:8080](http://127.0.0.1:8080) — должна открыться демо-страница из `sites/demo`.

## Конфиг

```toml
listen = "0.0.0.0:8080"

[[site.localhost]]
root = "sites/demo"

[[site.api]]
proxy = "http://127.0.0.1:3000"
```

- Имя в `[[site.имя]]` — значение `Host` без порта (регистр не важен).
- У каждого сайта **либо** `root`, **либо** `proxy`, не оба.
- Пути `root` относительные — от каталога, где лежит `relay.toml`.

Проверка без запуска:

```bash
relay check --config relay.toml
```

## Структура проекта

```
include/relay/     заголовки
src/
  config/          разбор конфига
  http/            парсер, MIME
  handlers/        static + proxy
  net/             accept, соединения
  router/          выбор сайта
sites/demo/        пример статики
config/            пример конфига
```

Сеть — [Asio](https://think-async.com/Asio/) (standalone), однопоточный `io_context`. Прокси блокирующий внутри worker-логики соединения; для MVP с одним backend этого достаточно.

## Дальше по желанию

- TLS (OpenSSL / mbedTLS);
- reload конфига по SIGHUP;
- пул потоков и неблокирующий proxy;
- CLI `relay site add ...`.

## Лицензия

MIT — см. [LICENSE](LICENSE).

## GitHub (open source)

1. Зарегистрируйся на [github.com](https://github.com), установи [Git](https://git-scm.com/download/win).
2. В каталоге `relay`:

```bash
git init -b main
git add .
git commit -m "Initial commit: relay HTTP server"
```

3. На GitHub: **New repository** → имя, например `relay` → **Public** → без README (он уже есть локально).
4. Привязка и push (подставь свой логин):

```bash
git remote add origin https://github.com/YOUR_USER/relay.git
git push -u origin main
```

При первом push GitHub попросит войти (браузер или [Personal Access Token](https://github.com/settings/tokens) вместо пароля).

Чтобы другие могли собирать: в описании репозитория укажи «C++20, CMake, MIT», добавь topics `http-server`, `cpp`, `asio`, `reverse-proxy`.
