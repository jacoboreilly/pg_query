require 'digest'

class PgQuery
  def fingerprint # rubocop:disable Metrics/CyclomaticComplexity
    normalized_parsetree = deep_dup(parsetree)

    # First delete all simple elements and attributes that can be removed
    treewalker! normalized_parsetree do |expr, k, v|
      if v.is_a?(Hash) && %w(A_CONST ALIAS PARAMREF).include?(v.keys[0])
        # Remove constants, aliases and param references from tree
        expr[k] = nil
      elsif k == 'location'
        # Remove location info in order to ignore whitespace and target list ordering
        expr.delete(k)
      end
    end

    # Now remove all unnecessary info
    treewalker! normalized_parsetree do |expr, k, v|
      if k == 'AEXPR IN' && v.is_a?(Hash) && v['rexpr'].is_a?(Array)
        # Compact identical IN list elements to one
        v['rexpr'].uniq!
      elsif k == 'targetList' && v.is_a?(Array)
        # Remove SELECT target list names & ignore order
        v.each { |v2| v2['RESTARGET']['name'] = nil if v2['RESTARGET'] } # Remove names
        v.sort_by!(&:to_s)
        expr[k] = v
      elsif k == 'cols' && v.is_a?(Array)
        # Ignore INSERT cols order
        v.sort_by!(&:to_s)
        expr[k] = v
      end
    end

    Digest::SHA1.hexdigest(normalized_parsetree.to_s)
  end

  def fingerprint_new(hash: Digest::SHA1.new) # rubocop:disable Metrics/CyclomaticComplexity
    exprs = parsetree.dup

    loop do
      expr = exprs.shift

      if expr.is_a?(Hash)
        expr.sort_by { |k, _| k }.reverse_each do |k, v|
          next if %w(A_Const Alias ParamRef location).include?(k)

          if v.is_a?(Hash)
            exprs.unshift(v)
          elsif v.is_a?(Array)
            exprs = v + exprs
          elsif !v.nil?
            exprs.unshift(v)
          end

          exprs.unshift(k) if k[/^[A-Z]+/]
        end
      elsif expr.is_a?(Array)
        exprs = expr + exprs
      else
        hash.update expr.to_s
      end

      break if exprs.empty?
    end

    hash.hexdigest
  end
end
