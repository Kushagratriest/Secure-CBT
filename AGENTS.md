# Project rules for Secure-CBT

1. Before writing any code, read every existing file in the repo relevant 
   to the task, not just the one being edited. State which files you read.

2. When you write or complete a file, after the code block, give a full 
   explanation covering:
   - What this file's job is in the overall pipeline
   - Walk through the code top to bottom, section by section, explaining 
     WHY each part exists, not just what it does
   - Every function: what calls it, what it returns, and what happens if 
     it fails
   - Any edge cases you handled and why they matter
   - How this file connects to the other files in the repo (what it 
     includes, what includes it, what data flows in and out)

3. Never silently touch a file outside the one you were asked to edit. 
   If a change elsewhere is needed, stop and say so instead of doing it.

4. Never change existing working logic unless explicitly asked. Flag 
   bugs you notice, but don't fix them unless told to.

5. Do not invent requirements, test results, or statuses that haven't 
   been verified by running the code.

6. When done, end with a short "entry point to exit point" trace: if this 
   function were called from main.cpp right now, trace the full call 
   path step by step.

7. When you write code, add a short comment on each line (or small group 
   of closely related lines) saying what that specific line does, in 
   plain language a 3rd-semester student can follow.
