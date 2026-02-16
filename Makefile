libraries/AceCommon:
	@echo "Installing AceCommon..."
	@for i in 1 2 3; do \
		rm -rf libraries/package-* || true; \
		if $(CLICFG) lib install AceCommon@1.6.2; then \
			echo "✓ AceCommon installed successfully"; \
			break; \
		else \
			if [ $$i -lt 3 ]; then \
				wait_time=$$((2 ** $$i)); \
				echo "⚠ AceCommon install failed (attempt $$i/3), retrying in $${wait_time}s..."; \
				sleep $$wait_time; \
			else \
				echo "✗ AceCommon install failed after 3 attempts"; \
				exit 1; \
			fi; \
		fi; \
	done

libraries/AceSorting:
	@echo "Installing AceSorting..."
	@for i in 1 2 3; do \
		rm -rf libraries/package-* || true; \
		if $(CLICFG) lib install AceSorting@1.0.0; then \
			echo "✓ AceSorting installed successfully"; \
			break; \
		else \
			if [ $$i -lt 3 ]; then \
				wait_time=$$((2 ** $$i)); \
				echo "⚠ AceSorting install failed (attempt $$i/3), retrying in $${wait_time}s..."; \
				sleep $$wait_time; \
			else \
				echo "✗ AceSorting install failed after 3 attempts"; \
				exit 1; \
			fi; \
		fi; \
	done

libraries/AceTime:
	@echo "Installing AceTime..."
	@for i in 1 2 3; do \
		rm -rf libraries/package-* || true; \
		if $(CLICFG) lib install AceTime@2.0.1; then \
			echo "✓ AceTime installed successfully"; \
			break; \
		else \
			if [ $$i -lt 3 ]; then \
				wait_time=$$((2 ** $$i)); \
				echo "⚠ AceTime install failed (attempt $$i/3), retrying in $${wait_time}s..."; \
				sleep $$wait_time; \
			else \
				echo "✗ AceTime install failed after 3 attempts"; \
				exit 1; \
			fi; \
		fi; \
	done
