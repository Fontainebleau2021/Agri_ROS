const fs = require('fs')
const path = require('path')
const { app, BrowserWindow, globalShortcut, protocol, Menu, dialog } = require('electron')
const { download } = require('electron-dl')

const express = require('express')
const Admzip = require('adm-zip')
const rimraf = require('rimraf')

const EPApp = express()
EPApp.use(express.json())
EPApp.use(express.urlencoded())

// const port = 3290
const store = {}

EPApp.get('/:key', (req, res) => {
  res.send(store[req.params.key])
})

EPApp.post('/', function (req, res) {
  const data = req.body
  Object.assign(store, data)
  res.send('data is stored!')
})

const EPServer = null
let cachePath = ''
let toolPath = ''

const windows = new Set()
console.log(__dirname)

if (process.env.NODE_ENV === 'development') {
  cachePath = path.join(__dirname, './core')
  toolPath = path.join(__dirname, './core/tools')
  process.env.PATH = path.join(__dirname, './core/Interpreter/Scripts') + ';' +
                  path.join(__dirname, './core/Interpreter') + ';' + process.env.PATH + ';' +
                  path.join(__dirname, './core/Interpreter/Lib')
} else {
  cachePath = path.join(__dirname, '../../core')
  toolPath = path.join(__dirname, '../../core/tools')
  process.env.PATH = path.join(__dirname, '../../core/Interpreter/Scripts') + ';' + path.join(__dirname, '../../core/Interpreter') + ';' + process.env.PATH
}

const deleteFolderContent = function (path) {
  if (fs.existsSync(path)) {
    const files = fs.readdirSync(path)
    files.forEach((file) => {
      const curPath = path + '/' + file
      if (fs.statSync(curPath).isDirectory()) {
        deleteFolderContent(curPath) // 递归删除文件夹
        fs.rmdirSync(curPath)
      } else {
        fs.unlinkSync(curPath) // 删除文件
      }
    })
  }
}

const menuTemplate = [
  {
    label: '文件',
    submenu: [
      {
        label: '打开',
        accelerator: 'CmdOrCtrl+O',
        click () {
          dialog.showOpenDialog({
            properties: ['打开模型'],
            filters: [
              { name: '模型文件', extensions: ['lab'] },
              { name: '所有类型', extensions: ['*'] }
            ]
          }).then(({ filePaths }) => {
            if (filePaths) {
              const file = filePaths[0]
              fs.readFile(file, 'utf8', function (err, data) {
                if (err) {
                  dialog.showErrorBox('打开模型错误', err.message)
                  return
                }
                const model = data.toString()
                const win = createWindow()
                setTimeout(() => {
                  win.webContents.send('init-model', model)
                }, 1000)
              })
            }
          })
        }
      },
      {
        type: 'separator'
      },
      {
        label: '新建项目',
        accelerator: 'CmdOrCtrl+N',
        click () {
          const model = JSON.stringify({
            env: 'development'
          })
          const win = createWindow()
          setTimeout(() => {
            win.webContents.send('init-model', model)
          }, 1000)
        }
      },
      // {
      //   label: '新建应用模型',
      //   accelerator: 'CmdOrCtrl+P',
      //   click () {
      //     const model = JSON.stringify({
      //       env: 'production'
      //     })
      //     const win = createWindow()
      //     setTimeout(() => {
      //       win.webContents.send('init-model', model)
      //     }, 1000)
      //   }
      // },
      {
        type: 'separator'
      },
      {
        label: '清空应用缓存',
        click () {
          const index = dialog.showMessageBoxSync({
            type: 'info',
            title: '操作提示',
            message: '确认清空应用缓存',
            buttons: ['清除', '取消']
          })

          if (index === 0) {
            let files = []
            const path = cachePath + '/cache'
            if (fs.existsSync(path)) {
              files = fs.readdirSync(path)
              files.forEach((file) => {
                const curPath = path + '/' + file
                fs.unlink(curPath, err => {
                  if (err) {
                    console.error(err)
                  }
                })
              })
            }
          }
        }
      },
      {
        label: '清空图片缓存',
        click () {
          const index = dialog.showMessageBoxSync({
            type: 'info',
            title: '操作提示',
            message: '确认清空图片缓存',
            buttons: ['清除', '取消']
          })

          if (index === 0) {
            const path = cachePath + '/store/imgs'
            deleteFolderContent(path)
          }
        }
      },
      {
        type: 'separator'
      },
      {
        label: '保存',
        accelerator: 'CmdOrCtrl+S',
        click () {
          const currentWindow = BrowserWindow.getFocusedWindow()
          if (currentWindow) {
            currentWindow.webContents.send('save-model')
          }
        }
      },
      {
        label: '生成代码',
        click () {
          const currentWindow = BrowserWindow.getFocusedWindow()
          if (currentWindow) {
            currentWindow.webContents.send('save-run-config')
          }
        }
      }
    ]
  },
  // {
  //   label: '服务',
  //   submenu: [
  //     {
  //       id: 'open-serve',
  //       label: '开启数据服务[3290]',
  //       enabled: true,
  //       click () {
  //         EPServer = EPApp.listen(port, () => {
  //           console.log('数据服务器已经启动,端口：3290')
  //         })
  //         const appMenu = Menu.getApplicationMenu()
  //         appMenu.getMenuItemById('open-serve').enabled = false
  //         appMenu.getMenuItemById('close-serve').enabled = true
  //       }
  //     },
  //     {
  //       id: 'close-serve',
  //       label: '关闭数据服务',
  //       enabled: false,
  //       click () {
  //         EPServer && EPServer.close()
  //         EPServer = null
  //         const appMenu = Menu.getApplicationMenu()
  //         appMenu.getMenuItemById('open-serve').enabled = true
  //         appMenu.getMenuItemById('close-serve').enabled = false
  //       }
  //     }

  //   ]
  // },
  {
    label: '工具',
    submenu: [
      {
        id: 'label-img',
        label: '目标检测标注',
        click () {
          const exePath = toolPath + '/labelImg.exe'
          const { execFile } = require('child_process')
          execFile(exePath, function (err, data) {
            if (err) {
              console.error(err)
              return
            }
            console.log(data.toString())
          })
        }
      },
      {
        id: 'label-me',
        label: '图像分割标注',
        click () {
          const exePath = toolPath + '/Labelme.exe'
          const { execFile } = require('child_process')
          execFile(exePath, function (err, data) {
            if (err) {
              console.error(err)
              return
            }
            console.log(data.toString())
          })
        }
      }

    ]
  },
  {
    label: '帮助',
    submenu: [
      {
        label: '命令行',
        accelerator: 'CmdOrCtrl+T',
        click () {
          const currentWindow = BrowserWindow.getFocusedWindow()
          const win = new BrowserWindow({
    icon: path.join(__dirname, "assets", "app_icon.png"),
            parent: currentWindow,
            width: 1000,
            height: 800,
            webPreferences: {
              webSecurity: false,
              nodeIntegration: true,
              enableRemoteModule: true
            }
          })

          win.setMenu(null)
          win.loadURL(`file://${__dirname}/dist/term.html`)
        }
      },
      {
        id: 'update-model',
        label: '更新所有模块',
        click () {
          const currentWindow = BrowserWindow.getFocusedWindow()
          download(currentWindow, 'http://wuchuanxun.com:3364/modules.zip', {
            directory: app.getAppPath() + '/core',
            onCompleted: function ({ path }) {
              dialog.showMessageBox({
                type: 'warning',
                title: '模块更新',
                message: '下载完成，确认更新？',
                buttons: ['确认', '取消']
              }).then(result => {
                if (result.response === 0) {
                  deleteFolderContent(app.getAppPath() + '/core/model')

                  const unzip = new Admzip(path)
                  unzip.extractAllTo(app.getAppPath() + '/core/model', true)

                  dialog.showMessageBox(currentWindow, {
                    message: '更新模块完成，请重启客户端'
                  })
                }
                rimraf(path, (err) => {
                  if (err) { console.log(err) }
                })
              })
            }
          }).catch(() => {
            dialog.showMessageBox(currentWindow, {
              message: '暂无可用更新！'
            })
          })
        }
      },
      {
        label: '关于',
        click () {
          const currentWindow = BrowserWindow.getFocusedWindow()
          const win = new BrowserWindow({
    icon: path.join(__dirname, "assets", "app_icon.png"),
            title: '关于',
            parent: currentWindow,
            width: 600,
            height: 400,
            webPreferences: {
              webSecurity: false,
              nodeIntegration: true,
              enableRemoteModule: true
            }
          })

          win.setMenu(null)
          win.loadURL(`file://${__dirname}/dist/about.html`)
        }
      }
    ]
  }
]

app.whenReady().then(() => {
  protocol.registerFileProtocol('file', (request, callback) => {
    const pathname = decodeURI(request.url.replace('file:///', ''))
    callback(pathname)
  })
})

app.allowRendererProcessReuse = false

// 声明变量
const winURL = process.env.NODE_ENV === 'development'
  ? `http://localhost:8080`
  : `file://${__dirname}/dist/index.html`

function createWindow () {
  const win = new BrowserWindow({
    icon: path.join(__dirname, "assets", "app_icon.png"),
    show: false,
    webPreferences: {
      webSecurity: false,
      nodeIntegration: true,
      enableRemoteModule: true
    }
  })

  win.maximize()

  win.loadURL(winURL)

  // 展示窗体
  win.once('ready-to-show', () => win.show())

  win.on('closed', () => {
    windows.delete(win)
  })

  // 全屏
  globalShortcut.register('F11', function () {
    if (win.isFullScreen()) {
      win.setFullScreen(false)
    } else {
      win.setFullScreen(true)
    }
  })
  // 调试
  globalShortcut.register('CTRL+SHIFT+I', function () {
    win.webContents.openDevTools()
  })
  windows.add(win)
  return win
}

app.on('ready', () => {
  const appMenu = Menu.buildFromTemplate(menuTemplate)
  Menu.setApplicationMenu(appMenu)

  createWindow()
  if (process.env.NODE_ENV === 'development') {
    require('vue-devtools').install()
  }
})

app.on('will-quit', () => {
  globalShortcut.unregister('F11')
  globalShortcut.unregister('CTRL+SHIFT+I')
  globalShortcut.unregisterAll()
})

app.on('window-all-closed', () => {
  let files = []
  const path = cachePath + '/cache'
  if (fs.existsSync(path)) {
    files = fs.readdirSync(path)
    files.forEach((file) => {
      const curPath = path + '/' + file
      fs.unlink(curPath, err => {
        if (err) {
          console.error(err)
        }
      })
    })
  }

  deleteFolderContent(cachePath + '/store/imgs')

  if (process.platform !== 'darwin') {
    app.quit()
  }
  EPServer && EPServer.close()
})
