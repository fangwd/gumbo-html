const https = require("https");
const fs = require("fs");
const os = require("os");
const path = require("path");

const libs = "https://raw.githubusercontent.com/fangwd/gumbo-html/main/libs";
const url = `${libs}/${process.platform}/${os.arch()}/html.node`;
const file = fs.createWriteStream(path.join(__dirname, "html.node"));

https.get(url, (response) => {
  response.pipe(file);
  file.on("finish", () => {
    file.close();
  });
});
