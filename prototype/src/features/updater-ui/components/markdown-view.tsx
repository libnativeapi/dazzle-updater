import type { ReactNode } from 'react'

import { parseMarkdown, type Inline } from '../markdown'

import './markdown-view.css'

/**
 * Read-only rich text rendered from the Markdown subset the spec supports, as
 * the `NSTextView` (macOS) or RichEdit control (Windows) would show it — what
 * the update window shows its release notes in. A link is handed to the
 * system browser rather than followed in the window.
 */
export function MarkdownView({ markdown }: { markdown: string }) {
  return (
    <div className="markdown-view">
      {parseMarkdown(markdown).map((block, index) => {
        switch (block.type) {
          case 'heading': {
            const Heading = `h${Math.min(block.level + 1, 6)}` as 'h2'
            return <Heading key={index}>{renderInline(block.children)}</Heading>
          }
          case 'paragraph':
            return <p key={index}>{renderInline(block.children)}</p>
          case 'list': {
            const List = block.ordered ? 'ol' : 'ul'
            return (
              <List key={index}>
                {block.items.map((item, itemIndex) => (
                  <li key={itemIndex}>{renderInline(item)}</li>
                ))}
              </List>
            )
          }
          case 'code':
            return <pre key={index}>{block.text}</pre>
        }
      })}
    </div>
  )
}

function renderInline(nodes: Inline[]): ReactNode[] {
  return nodes.map((node, index) => {
    switch (node.type) {
      case 'text':
        return node.text
      case 'strong':
        return <strong key={index}>{renderInline(node.children)}</strong>
      case 'em':
        return <em key={index}>{renderInline(node.children)}</em>
      case 'code':
        return <code key={index}>{node.text}</code>
      case 'link':
        return (
          // The prototype opens nothing; the title shows where the browser would go.
          <a key={index} href={node.href} title={node.href} onClick={event => event.preventDefault()}>
            {renderInline(node.children)}
          </a>
        )
    }
  })
}
