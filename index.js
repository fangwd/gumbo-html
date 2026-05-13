'use strict';

const bindings = require('bindings');
const html = bindings('html');
const { enhance } = require('./lib/wrapper');

module.exports = enhance(html);
