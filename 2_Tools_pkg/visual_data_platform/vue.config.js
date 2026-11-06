module.exports = {
  devServer: {
    host: 'localhost'
  },
  pages: {
    index: {
      entry: 'src/main.js',
      template: 'public/index.html',
      filename: 'index.html'
    },
    about: {
      entry: 'src/about.js',
      template: 'public/about.html',
      filename: 'about.html'
    },
    term: {
      entry: 'src/term.js',
      template: 'public/term.html',
      filename: 'term.html'
    }
  },
  publicPath: './',
  lintOnSave: false,
  productionSourceMap: true
}
