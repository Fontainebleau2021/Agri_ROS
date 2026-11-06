import Vue from 'vue'
import NodeVisualization from '@/layout/resultDisplay'
import PyScriptModal from '@/components/pyScriptModal'
import ImageCrop from '@/components/imageCrop'
import ImageLabelTool from '@/components/imageLabelTool'
import JsonSchemaModal from '@/components/jsonSchemaModal'
import ExcelEditor from '@/components/excelEditor'

const Plugins = {
  install: function (Vue) {
    const NodeVisualizationClass = Vue.extend(NodeVisualization)
    const PyScriptModalClass = Vue.extend(PyScriptModal)
    const JsonSchemaModalClass = Vue.extend(JsonSchemaModal)
    const ImageCropClass = Vue.extend(ImageCrop)
    const ImageLabelToolClass = Vue.extend(ImageLabelTool)
    const ExcelEditorClass = Vue.extend(ExcelEditor)

    Vue.prototype.$NodeVisualization = function (propsData) {
      new NodeVisualizationClass({ propsData }).$mount()
    }
    Vue.prototype.$PyScriptModal = function (propsData) {
      new PyScriptModalClass({ propsData }).$mount()
    }
    Vue.prototype.$JsonSchemaModal = function (propsData) {
      new JsonSchemaModalClass({ propsData }).$mount()
    }
    Vue.prototype.$ImageCrop = function (propsData) {
      new ImageCropClass({ propsData }).$mount()
    }
    Vue.prototype.$ImageLabelTool = function (propsData) {
      new ImageLabelToolClass({ propsData }).$mount()
    }
    Vue.prototype.$ExcelEditor = function (propsData) {
      new ExcelEditorClass({ propsData }).$mount()
    }
  }
}

Vue.use(Plugins)
