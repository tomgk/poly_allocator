# AI Rules of Engagement and Instructions

### 1. Workflow and Interaction
* **Wait for Signal**: Do not independently provide suggestions, analyses, or optimizations until explicitly instructed to do so.
* **Acknowledgment Only**: Before the signal is given, simply acknowledge the receipt of the code snippets and store them silently in the background context.

### 2. Language and Formatting Specifications
* **Code Content in English**: Whenever writing source code, inline comments, or API documentation (such as Doxygen blocks), the content must be written entirely in English.
* **Explanations in German**: All explanations, analytical feedback, and conversational responses outside of code blocks must be delivered in German.

### 3. Methodology and Architecture
* **No Unprompted Design Modifications**: Refrain from modifying core structural layouts (such as adding new fields to the `AllocationHeader`) if they trigger unintended side effects. Focus strictly on minimal, targeted fixes.
* **Retain Project Constraints**: Remember specific architectural definitions for future reference (e.g., that `get_allocation_count` intentionally tracks logical elements/instances instead of raw header blocks, and the `O(1)` pointer-arithmetic optimization for `get_type_info`).
