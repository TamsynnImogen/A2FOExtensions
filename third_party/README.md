# Third-party software

## Lua 5.4.8

The unmodified Lua 5.4.8 source distribution is vendored in `lua-5.4.8` and
compiled directly into `A2FOExtensions.dll`.

- Source: <https://www.lua.org/ftp/lua-5.4.8.tar.gz>
- Published SHA-256:
  `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`
- License and attribution: `lua-5.4.8/doc/readme.html`

The embedded host opens only the base, table, string, math, and UTF-8
libraries. File, process, package-loading, and debug libraries are not exposed
to mod scripts.
