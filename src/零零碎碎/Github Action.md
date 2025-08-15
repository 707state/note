-   [Github Action](#github-action)

# Github Action

GitHub Actions 是一种持续集成和持续交付 (CI/CD)平台，可用于自动执行生成、测试和部署管道。

你可以创建工作流，以便在推送更改到存储库时运行测试，或将合并的拉取请求部署到生产环境。

工作环境(.github/workflow)这里面定义的是一些工作流，这些工作流可以用来执行一些预设的操作，诸如编译、部署等。

Github
Action是用YAML来定义的一套模板，触发工作流等都可以通过定义变量来触发。

记录这个只是因为mdbook+gh page要用到，顺便了解一下。

``` yaml
name: Deploy
on:
  push:
    branches:
      - main

jobs:
  deploy:
    runs-on: ubuntu-latest
    permissions:
      contents: write  # To push a branch
      pages: write  # To push to a GitHub Pages site
      id-token: write # To update the deployment status
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      - name: Install latest mdbook
        run: |
          tag=$(curl 'https://api.github.com/repos/rust-lang/mdbook/releases/latest' | jq -r '.tag_name')
          url="https://github.com/rust-lang/mdbook/releases/download/${tag}/mdbook-${tag}-x86_64-unknown-linux-gnu.tar.gz"
          mkdir mdbook
          curl -sSL $url | tar -xz --directory=./mdbook
          echo `pwd`/mdbook >> $GITHUB_PATH
      - name: Build Book
        run: |
          # This assumes your book is in the root of your repository.
          # Just add a `cd` here if you need to change to another directory.
          mdbook build
      - name: Setup Pages
        uses: actions/configure-pages@v4
      - name: Upload artifact
        uses: actions/upload-pages-artifact@v3
        with:
          # Upload entire repository
          path: 'book'
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```
