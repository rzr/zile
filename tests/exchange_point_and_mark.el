(forward-char)
(forward-char)
(forward-char)
(forward-char)
(set-mark (point))
(forward-line)
(forward-line)
(exchange-point-and-mark)
(insert "f")
(save-buffer)
(save-buffers-kill-emacs)