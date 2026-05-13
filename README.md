# gumbo-html

CSS selector based on Gumbo HTML parser.

## Installation

```sh
npm install gumbo-html
```

`gumbo-html` is a native Node.js addon and is compiled from source on install.
You'll need the standard `node-gyp` toolchain:

- Python 3
- A C/C++ compiler (Xcode Command Line Tools on macOS, `build-essential` on
  Linux, or Visual Studio Build Tools on Windows)

See the [node-gyp docs](https://github.com/nodejs/node-gyp#installation) for
platform-specific setup details.

## Usage

```ts
import { parse } from 'gumbo-html';

const html = `
<html>
  <p class="foo bar blah">Foo</p>
  <p class="bar">Bar</p>
</html>
`;

const doc = parse(html);

doc.find('.bar').forEach((el) => {
  console.log(el.innerText);
});
```

## License

MIT. Bundles [google/gumbo-parser](https://github.com/google/gumbo-parser)
(Apache-2.0) under `src/gumbo-parser/`.
