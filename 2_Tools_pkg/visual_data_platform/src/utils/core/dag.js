function getDagOrder (preRelations) {
  const iters = []
  const keys = Object.keys(preRelations)
  while (iters.length < keys.length) {
    for (const key of keys) {
      if (iters.includes(key)) continue

      const deps = preRelations[key]

      let enabled = true
      for (const dep of deps) {
        if (!keys.includes(dep)) continue

        if (!iters.includes(dep)) {
          enabled = false
          break
        }
      }

      if (enabled) {
        iters.push(key)
      }
    }
  }
  return iters
}

function getSourceNodesRecursive (node) {
  const nodes = new Set()
  nodes.add(node)

  for (const sNode of node.getNeighbors('source')) {
    for (const item of getSourceNodesRecursive(sNode)) {
      nodes.add(item)
    }
  }
  return nodes
}

function getTargetNodesRecursive (node) {
  const nodes = new Set()
  nodes.add(node)

  for (const sNode of node.getNeighbors('target')) {
    for (const item of getTargetNodesRecursive(sNode)) {
      nodes.add(item)
    }
  }
  return nodes
}

module.exports = {
  getDagOrder,
  getSourceNodesRecursive,
  getTargetNodesRecursive
}
