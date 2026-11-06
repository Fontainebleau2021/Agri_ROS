const fs = window.require('fs')
const { dialog } = window.require('electron').remote

const readFile = function (filePath) {
  return new Promise((resolve, reject) => {
    fs.readFile(filePath, 'utf8', function (err, data) {
      if (err) {
        reject(err)
        return
      }
      resolve(data.toString())
    })
  })
}

const saveFile = function (filePath, content) {
  return new Promise((resolve, reject) => {
    fs.writeFile(filePath, content, function (err) {
      if (err) {
        reject(err)
      }
      resolve(filePath)
    })
  })
}

const saveFileDialog = function (options, content) {
  return new Promise((resolve, reject) => {
    dialog.showSaveDialog(options)
      .then(file => {
        if (file.canceled) return

        fs.writeFile(file.filePath, content, function (err) {
          if (err) {
            reject(err)
          }
          resolve(file.filePath)
        })
      })
  })
}

const readFileDialog = function (options) {
  return new Promise((resolve, reject) => {
    dialog.showOpenDialog({
      properties: ['openFile'],
      filters: [
        ...options.filters
      ]
    }).then(({ filePaths }) => {
      if (filePaths) {
        const file = filePaths[0]
        fs.readFile(file, 'utf8', function (err, data) {
          if (err) {
            reject(err)
            return
          }
          resolve(data.toString())
        })
      }
    })
  })
}

const readFilePathDialog = function (options) {
  return new Promise((resolve, reject) => {
    dialog.showOpenDialog({
      properties: ['openFile'],
      ...options
    }).then(({ filePaths }) => {
      if (filePaths) {
        const file = filePaths[0]
        resolve(file)
      }
      resolve('')
    })
  })
}

const saveFilePathDialog = function (options) {
  return new Promise((resolve, reject) => {
    dialog.showSaveDialog(options)
      .then(file => {
        if (file.canceled) return
        resolve(file.filePath)
      })
  })
}

module.exports = {
  readFile,
  saveFile,
  saveFileDialog,
  readFileDialog,
  readFilePathDialog,
  saveFilePathDialog
}
