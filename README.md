CSS selector based on Gumbo HTML parser.

## Installation
```
$ npm install gumbo-html
```

## Usage

Example:
```ts
import {parse} from 'gumbo-html';

const html = ` 
<html>
  <p class="foo bar blah">Foo</p>
  <p class="bar">Bar</p>
</html>
`

const xdoc = parse(html);

xdoc.find('.bar').forEach(el => {
  console.log(el.innerText)
});
```