const solveFunction = function (abj) {
  for (const key in abj) {
    const value = abj[key]
    if (typeof (value) === 'string' && value.startsWith('$function$')) {
      const lArgIdx = value.indexOf('(')
      const rArgIdx = value.indexOf(')')
      const args = value.substring(lArgIdx + 1, rArgIdx).split(',')
      const lBodyIdx = value.indexOf('{')
      const rBodyIdx = value.length - 1
      abj[key] = new Function(...args, value.substring(lBodyIdx + 1, rBodyIdx))
    }
    if (value instanceof Array || value instanceof Object) {
      solveFunction(value)
    }
  }
}

const parseJson = function (jsonStr) {
  const data = JSON.parse(jsonStr)
  if (jsonStr.includes('$function$')) {
    if (data instanceof Array || data instanceof Object) {
      solveFunction(data)
    }
  }
  return data
}

export default parseJson
