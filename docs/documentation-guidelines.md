# Documentation Guidelines

The subject requires the project, **including its documentation**, to be accessible to people with
disabilities. These rules apply to every page in `docs/`, the README and the CONTRIBUTING guide.
They follow the [Web Content Accessibility Guidelines (WCAG) 2.2](https://www.w3.org/TR/WCAG22/), level AA.

## Structure

| Rule | Why |
|---|---|
| One `#` title per page, then `##` and `###` in order, without skipping a level | Screen readers let users jump from heading to heading; a clean outline is the page's table of contents. |
| Short sections and short paragraphs | Easier to read for everyone, especially for people with dyslexia or attention disorders. |
| Lists and tables for steps and comparisons | Structure is announced by screen readers ("list, 4 items"). |
| Tables always have a header row | Screen readers read the column name before each cell. |
| Code in fenced blocks with a language (` ```cpp `) | Code is announced as code and highlighted for readers with low vision. |

## Images and diagrams

| Rule | Why |
|---|---|
| Every image has alternative text: `![Server tick loop](images/loop.png)` | Blind readers hear the alternative text instead of the image. |
| Every diagram is followed by a *text description* in italics | A Mermaid diagram has no alternative text; the description gives the same information. |
| Prefer Mermaid diagrams to screenshots of diagrams | Text-based diagrams adapt to dark mode and zoom without becoming blurry. |
| No text inside images when it can be written in the page | Image text cannot be searched, translated, enlarged or read aloud. |

## Language and links

| Rule | Why |
|---|---|
| Simple English, short sentences, one idea per sentence | Many readers, including the team, are not native English speakers. Also helps readers with cognitive disabilities. |
| Explain an acronym the first time it appears on a page (for example "Entity-Component-System (ECS)") | Readers may arrive on any page from a search engine. |
| Link text says where it goes: "see the [protocol specification](protocol.md)", never "click [here](protocol.md)" | Screen reader users often list the links of a page out of context. |

## Color and contrast

| Rule | Why |
|---|---|
| Never give information by color alone (in diagrams, use labels or shapes as well) | Color-blind readers cannot tell some colors apart. |
| Keep the default theme colors of the documentation site, which meet the WCAG contrast ratio (4.5:1 for text) | Low-vision readers need enough contrast between text and background. |
| The documentation site must offer a dark mode and stay readable at 200% zoom | Some readers are sensitive to bright screens; others enlarge the page. |

## Checklist before opening a pull request that changes documentation

- [ ] Headings are in order, one `#` per page.
- [ ] Every image has alternative text; every diagram has a text description.
- [ ] No information is given by color only.
- [ ] Link texts make sense out of context.
- [ ] New acronyms are explained.
