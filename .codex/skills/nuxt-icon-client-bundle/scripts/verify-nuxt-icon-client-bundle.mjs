#!/usr/bin/env node
import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs'
import path from 'node:path'

const COLLECTIONS = ['simple-icons', 'heroicons', 'lucide', 'carbon', 'mdi']

function findFrontendDir() {
  const cwd = process.cwd()
  if (existsSync(path.join(cwd, 'nuxt.config.ts')) && existsSync(path.join(cwd, 'app'))) {
    return cwd
  }

  const nested = path.join(cwd, 'frontend')
  if (existsSync(path.join(nested, 'nuxt.config.ts')) && existsSync(path.join(nested, 'app'))) {
    return nested
  }

  throw new Error('Cannot find frontend directory. Run from repo root or frontend/.')
}

function walkFiles(dir, extensions, output = []) {
  for (const entry of readdirSync(dir)) {
    if (entry === 'node_modules' || entry === '.nuxt' || entry === '.output') {
      continue
    }

    const fullPath = path.join(dir, entry)
    const stat = statSync(fullPath)
    if (stat.isDirectory()) {
      walkFiles(fullPath, extensions, output)
      continue
    }

    if (extensions.some(extension => fullPath.endsWith(extension))) {
      output.push(fullPath)
    }
  }

  return output
}

function extractStaticIconNames(frontendDir) {
  const appDir = path.join(frontendDir, 'app')
  const sourceFiles = walkFiles(appDir, ['.vue', '.ts', '.tsx', '.js', '.jsx'])
  const escapedCollections = COLLECTIONS
    .sort((a, b) => b.length - a.length)
    .map(collection => collection.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'))
    .join('|')
  const iconPattern = new RegExp(`i-(${escapedCollections})-([a-z0-9][a-z0-9-]*)`, 'g')
  const icons = new Set()

  for (const file of sourceFiles) {
    const source = readFileSync(file, 'utf8')
    for (const match of source.matchAll(iconPattern)) {
      icons.add(`${match[1]}:${match[2]}`)
    }
  }

  return [...icons].sort()
}

function parseBundleIcons(frontendDir) {
  const bundlePath = path.join(frontendDir, '.nuxt', 'nuxt-icon-client-bundle.mjs')
  if (!existsSync(bundlePath)) {
    throw new Error(`Missing ${bundlePath}. Run npm run typecheck or start Nuxt once to generate it.`)
  }

  const source = readFileSync(bundlePath, 'utf8')
  const match = source.match(/JSON\.parse\("([\s\S]*)"\)/)
  if (!match) {
    throw new Error(`Cannot parse generated Nuxt Icon client bundle: ${bundlePath}`)
  }

  const collections = JSON.parse(JSON.parse(`"${match[1]}"`))
  const bundled = new Set()
  for (const collection of collections) {
    for (const iconName of Object.keys(collection.icons ?? {})) {
      bundled.add(`${collection.prefix}:${iconName}`)
    }
  }

  return bundled
}

const frontendDir = findFrontendDir()
const scannedIcons = extractStaticIconNames(frontendDir)
const bundledIcons = parseBundleIcons(frontendDir)
const missingIcons = scannedIcons.filter(icon => !bundledIcons.has(icon))

console.log(`Scanned static Nuxt icons: ${scannedIcons.length}`)
console.log(`Bundled Nuxt icons: ${bundledIcons.size}`)

if (missingIcons.length > 0) {
  console.error('Missing icons from Nuxt Icon client bundle:')
  for (const icon of missingIcons) {
    console.error(`- ${icon}`)
  }
  process.exit(1)
}

console.log('All scanned static Nuxt icons are present in the client bundle.')
