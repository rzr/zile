(set-mark (point))
(forward-line)
(forward-line)
(forward-line)
(forward-line)
(shell-command-on-region (mark) (point) "sort" t)
(save-buffer)
(save-buffers-kill-emacs)