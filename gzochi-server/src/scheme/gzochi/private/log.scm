;; gzochi/private/log.scm: Private infrastructure for transactional log support
;; Copyright (C) 2016 Julian Graham
;;
;; gzochi is free software: you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;; 
;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

#!r6rs

(library (gzochi private log)
  (export gzochi:log-internal
	  gzochi:log-err-internal
	  gzochi:log-warning-internal
	  gzochi:log-notice-internal
	  gzochi:log-info-internal
	  gzochi:log-debug-internal

	  gzochi:log
	  gzochi:log-err
	  gzochi:log-warning
	  gzochi:log-notice
	  gzochi:log-info
	  gzochi:log-debug)

  (import (only (guile) cons* *unspecified* simple-format)
	  (rnrs base)
	  (rnrs conditions)
	  (rnrs enums)
	  (rnrs exceptions))

  (define-enumeration gzochi:log-priority
    (err warning info notice debug) 
    gzochi:make-log-priority-set)

  (define (gzochi:log-internal priority msg . args)
    (let ((formatted-msg (apply simple-format (cons* #f msg args))))
      (case priority
	((err) (primitive-log-internal 'err formatted-msg))
	((warning) (primitive-log-internal 'warning formatted-msg))
	((notice) (primitive-log-internal 'notice formatted-msg))
	((info) (primitive-log-internal 'info formatted-msg))
	((debug) (primitive-log-internal 'debug formatted-msg))
	(else (assertion-violation
	       'gzochi:log-internal "Invalid priority." priority)))))

  (define (gzochi:log-err-internal msg . args)
    (apply gzochi:log-internal (cons* (gzochi:log-priority err) msg args)))
  
  (define (gzochi:log-warning-internal msg . args)
    (apply gzochi:log-internal (cons* (gzochi:log-priority warning) msg args)))

  (define (gzochi:log-notice-internal msg . args)
    (apply gzochi:log-internal (cons* (gzochi:log-priority notice) msg args)))

  (define (gzochi:log-info-internal msg . args)
    (apply gzochi:log-internal (cons* (gzochi:log-priority info) msg args)))

  (define (gzochi:log-debug-internal msg . args)
    (apply gzochi:log-internal (cons* (gzochi:log-priority debug) msg args)))

  (define (gzochi:log priority msg . args)
    (let ((formatted-msg (apply simple-format (cons* #f msg args))))
      (case priority
	((err) (primitive-log 'err formatted-msg))
	((warning) (primitive-log 'warning formatted-msg))
	((notice) (primitive-log 'notice formatted-msg))
	((info) (primitive-log 'info formatted-msg))
	((debug) (primitive-log 'debug formatted-msg))
	(else (assertion-violation 'gzochi:log "Invalid priority." priority)))))

  (define (gzochi:log-err msg . args)
    (apply gzochi:log (cons* (gzochi:log-priority err) msg args)))
  
  (define (gzochi:log-warning msg . args)
    (apply gzochi:log (cons* (gzochi:log-priority warning) msg args)))

  (define (gzochi:log-notice msg . args)
    (apply gzochi:log (cons* (gzochi:log-priority notice) msg args)))

  (define (gzochi:log-info msg . args)
    (apply gzochi:log (cons* (gzochi:log-priority info) msg args)))

  (define (gzochi:log-debug msg . args)
    (apply gzochi:log (cons* (gzochi:log-priority debug) msg args)))

  (define primitive-log #f)
  (define primitive-log-internal #f)
)
