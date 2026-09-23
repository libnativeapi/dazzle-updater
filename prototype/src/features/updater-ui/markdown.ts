/**
 * The Markdown subset the window renders (specs/updater-ui.md): headings,
 * paragraphs, ordered and unordered lists, bold, italic, inline code, code
 * blocks and links. Images are dropped — no remote resource is loaded. The
 * prototype's stand-in for md4c and the HTML/RTF writers.
 */

export type Inline =
  | { type: 'text'; text: string }
  | { type: 'strong' | 'em'; children: Inline[] }
  | { type: 'code'; text: string }
  | { type: 'link'; href: string; children: Inline[] }

export type Block =
  | { type: 'heading'; level: number; children: Inline[] }
  | { type: 'paragraph'; children: Inline[] }
  | { type: 'list'; ordered: boolean; items: Inline[][] }
  | { type: 'code'; text: string }

const ITEM = /^\s*(?:([-*+])|(\d+)[.)])\s+(.*)$/

export function parseMarkdown(source: string): Block[] {
  const lines = source.replace(/\r\n?/g, '\n').split('\n')
  const blocks: Block[] = []
  let index = 0

  while (index < lines.length) {
    const line = lines[index] ?? ''

    if (line.trim() === '') {
      index += 1
      continue
    }

    if (line.startsWith('```')) {
      const code: string[] = []
      index += 1
      while (index < lines.length && !(lines[index] ?? '').startsWith('```')) code.push(lines[index++] ?? '')
      index += 1
      blocks.push({ type: 'code', text: code.join('\n') })
      continue
    }

    const heading = /^(#{1,6})\s+(.*)$/.exec(line)
    if (heading) {
      blocks.push({ type: 'heading', level: heading[1]!.length, children: parseInline(heading[2]!) })
      index += 1
      continue
    }

    const item = ITEM.exec(line)
    if (item) {
      const ordered = item[2] !== undefined
      const items: Inline[][] = []
      while (index < lines.length) {
        const next = ITEM.exec(lines[index] ?? '')
        if (!next || (next[2] !== undefined) !== ordered) break
        items.push(parseInline(next[3]!))
        index += 1
      }
      blocks.push({ type: 'list', ordered, items })
      continue
    }

    const paragraph: string[] = []
    while (index < lines.length) {
      const next = lines[index] ?? ''
      if (next.trim() === '' || next.startsWith('```') || /^#{1,6}\s/.test(next) || ITEM.test(next)) break
      paragraph.push(next.trim())
      index += 1
    }
    blocks.push({ type: 'paragraph', children: parseInline(paragraph.join(' ')) })
  }

  return blocks
}

const INLINE = /(!?)\[([^\]]*)\]\(([^)\s]+)\)|`([^`]+)`|\*\*(.+?)\*\*|__(.+?)__|\*(.+?)\*|_(.+?)_/

export function parseInline(text: string): Inline[] {
  const out: Inline[] = []
  let rest = text
  while (rest) {
    const match = INLINE.exec(rest)
    if (!match) {
      out.push({ type: 'text', text: rest })
      break
    }
    if (match.index > 0) out.push({ type: 'text', text: rest.slice(0, match.index) })
    const [whole, bang, label, href, code, strong, strong2, em, em2] = match
    if (href !== undefined) {
      // An image is dropped: its bytes would be a remote resource.
      if (!bang) out.push({ type: 'link', href, children: parseInline(label ?? '') })
    } else if (code !== undefined) {
      out.push({ type: 'code', text: code })
    } else if ((strong ?? strong2) !== undefined) {
      out.push({ type: 'strong', children: parseInline((strong ?? strong2)!) })
    } else {
      out.push({ type: 'em', children: parseInline((em ?? em2)!) })
    }
    rest = rest.slice(match.index + whole.length)
  }
  return out
}
