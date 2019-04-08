# NVM for Windows

Nodejs Version Manager for Windows. Single executable, written in VC++. Share npm global modules.

``` txt
Usage:

  nvm version
  nvm arch [x86 | x64]
  nvm cache [<npm cache dir>]
  nvm root [<node binary dir>]
  nvm list
  nvm use <version> [options]
  nvm uninstall <version>
  nvm install <version> [options]
  nvm node_mirror [default | taobao | <url>]
  nvm npm_mirror [default | taobao | <url>]

Options:

  --arch=<x86 | x64>
  --node_mirror=<default | taobao | <url>>
  --npm_mirror=<default | taobao | <url>>
  --cache=<npm cache dir>
  --root=<node binary dir>

```

## Quick Start

1. Uninstall Node.js installed by MSI installer and Remove `C:\Users\USERNAME\AppData\Roaming\npm`.
2. `mkdir somewhere-you-like`
3. Add `somewhere-you-like` to `%PATH%` manually.
4. Place `nvm.exe` in `somewhere-you-like`.
5. Start.

    ``` cmd
    > nvm use 10.15.3
    > node -v
    ```
